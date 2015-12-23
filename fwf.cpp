/**
 * @file fwf - Firewall filtyer test
 * - setup Firewall filter to use rules from rule file fw/ruleXX, where XX - number of sub-test
 * - execute queries for fw/passXX file, expect OK
 * - execute queries from fw/denyXX, expect Access Denied error (mysql_error 1141)
 * - repeat for all XX
 * - setup Firewall filter to block queries next 2 minutes using 'at_time' statement (see template fw/rules_at_time)
 * - start sending queries, expect Access Denied now and OK after two mintes
 * - setup Firewall filter to limit a number of queries during certain time
 * - start sending queries as fast as possible, expect OK for N first quries and Access Denied for next queries
 * - wait, start sending queries again, but only one query per second, expect OK
 * - try to load rules with syntax error, expect failure for all sessions and queries
 */

#include <my_config.h>
#include <iostream>
#include <ctime>
#include "testconnections.h"
#include "maxadmin_operations.h"
#include "sql_t1.h"

void copy_rules(TestConnections* Test, char * rules_name)
{
    char str[4096];
    Test->set_timeout(30);
    sprintf(str, "rm -rf %s/rules; mkdir %s/rules", Test->maxscale_access_homedir,  Test->maxscale_access_homedir);
    Test->tprintf("Creating rules dir: %s\n", str);
    Test->ssh_maxscale(str, TRUE);
    system(str);

    Test->set_timeout(30);
    sprintf(str, "scp -i %s -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null %s/fw/%s %s@%s:%s/rules/rules.txt", Test->maxscale_sshkey, Test->test_dir, rules_name, Test->maxscale_access_user, Test->maxscale_IP, Test->maxscale_access_homedir);
    Test->tprintf("Copying rules to Maxscale machine: %s\n", str);
    system(str);

    Test->set_timeout(30);
    sprintf(str, "chown maxscale:maxscale %s/rules -R", Test->maxscale_access_homedir);
    Test->tprintf("Copying rules to Maxscale machine: %s\n", str);
    Test->ssh_maxscale(str, TRUE);
}

int main(int argc, char *argv[])
{
    TestConnections * Test = new TestConnections(argc, argv);
    int local_result;
    char str[4096];
    char sql[4096];
    char pass_file[4096];
    char deny_file[4096];
    FILE* file;

    int N = 9;
    int i;

    for (i = 1; i < N+1; i++){
        Test->set_timeout(60);
        local_result = 0;

        Test->stop_maxscale();

        sprintf(str, "rules%d", i);
        copy_rules(Test, str);

        Test->start_maxscale();
        Test->connect_rwsplit();

        sprintf(pass_file, "%s/fw/pass%d", Test->test_dir, i);
        sprintf(deny_file, "%s/fw/deny%d", Test->test_dir, i);

        file = fopen(pass_file, "r");
        if (file != NULL) {
            Test->tprintf("********** Trying queries that should be OK ********** \n");
            while (fgets(sql, sizeof(sql), file)) {
                if (strlen(sql) > 1) {
                    Test->tprintf("%s", sql);
                    local_result += execute_query(Test->conn_rwsplit, sql);
                }
            }
            fclose(file);
        } else {
            Test->add_result(1, "Error opening query file\n");
        }

        file = fopen(deny_file, "r");
        if (file != NULL) {
            Test->tprintf("********** Trying queries that should FAIL ********** \n");
            while (fgets(sql, sizeof(sql), file)) {
                if (strlen(sql) > 1) {
                    Test->tprintf("%s", sql);
                    execute_query(Test->conn_rwsplit, sql);
                    if (mysql_errno(Test->conn_rwsplit) != 1141) {
                        Test->tprintf("Query succeded, but fail expected, errono is %d\n", mysql_errno(Test->conn_rwsplit));
                        local_result++;
                    }
                }
            }
            fclose(file);
        } else {
            Test->add_result(1, "Error opening query file\n");
        }
        Test->add_result(local_result, "********** rules%d test FAILED\n", i);
        mysql_close(Test->conn_rwsplit);
    }

    Test->set_timeout(60);
    Test->stop_maxscale();

    // Test for at_times clause
    Test->tprintf("Trying at_times clause\n");
    copy_rules(Test, (char *) "rules_at_time");

    sprintf(str, "start_time=`date +%%T`; stop_time=` date --date \"now +2 mins\" +%%T`; %s sed -i \"s/###time###/$start_time-$stop_time/\" %s/rules/rules.txt",
            Test->maxscale_access_sudo, Test->maxscale_access_homedir);
    Test->tprintf("DELETE quries without WHERE clause will be blocked during next 2 minutes\n");
    Test->tprintf("Put time to rules.txt: %s\n", str);
    Test->ssh_maxscale(str, FALSE);

    Test->start_maxscale();
    Test->connect_rwsplit();

    Test->tprintf("Trying 'DELETE FROM t1' and expecting FAILURE\n");
    execute_query(Test->conn_rwsplit, "DELETE FROM t1");
    if (mysql_errno(Test->conn_rwsplit) != 1141) {
        Test->add_result(1, "Query succeded, but fail expected, errono is %d\n", mysql_errno(Test->conn_rwsplit));
    }
    Test->tprintf("Waiting 3 minutes and trying 'DELETE FROM t1', expecting OK\n");
    Test->stop_timeout();
    sleep(180);
    Test->set_timeout(60);
    Test->try_query(Test->conn_rwsplit, "DELETE FROM t1");

    mysql_close(Test->conn_rwsplit);
    Test->stop_maxscale();

    Test->tprintf("Trying limit_queries clause\n");
    Test->tprintf("Copying rules to Maxscale machine: %s\n", str);
    copy_rules(Test, (char *) "rules_limit_queries");

    Test->start_maxscale();
    Test->connect_rwsplit();

    printf("Trying 10 quries as fast as possible\n");
    for (i = 0; i < 10; i++) {
        Test->add_result(execute_query(Test->conn_rwsplit, "SELECT * FROM t1"), "%d -query failed\n", i);
    }

    Test->tprintf("Expecting failures during next 5 seconds\n");

    time_t start_time_clock = time(NULL);
    timeval t1, t2;
    double elapsedTime;
    gettimeofday(&t1, NULL);


    do {
        gettimeofday(&t2, NULL);
        elapsedTime = (t2.tv_sec - t1.tv_sec);
        elapsedTime += (double) (t2.tv_usec - t1.tv_usec) / 1000000.0;
    } while ((execute_query(Test->conn_rwsplit, "SELECT * FROM t1") != 0) && (elapsedTime < 10));

    Test->tprintf("Quries were blocked during %f (using clock_gettime())\n", elapsedTime);
    Test->tprintf("Quries were blocked during %lu (using time())\n", time(NULL)-start_time_clock);
    if ((elapsedTime > 6) or (elapsedTime < 4)) {
        Test->add_result(1, "Queries were blocked during wrong time\n");
    }

    Test->set_timeout(60);
    printf("Trying 20 quries, 1 query / second\n");
    for (i = 0; i < 20; i++) {
        sleep(1);
        Test->add_result(execute_query(Test->conn_rwsplit, "SELECT * FROM t1"),"query failed\n");
        Test->tprintf("%d ", i);
    }
    Test->tprintf("\n");
    Test->set_timeout(60);
    Test->stop_maxscale();

    Test->tprintf("Trying rules with syntax error\n");
    Test->tprintf("Copying rules to Maxscale machine: %s\n", str);
    copy_rules(Test, (char *) "rules_syntax_error");

    Test->start_maxscale();
    Test->connect_rwsplit();

    Test->tprintf("Trying to connectt to Maxscale when 'rules' has syntax error, expecting failures\n");
    if (execute_query(Test->conn_rwsplit, "SELECT * FROM t1") == 0) {
        Test->add_result(1, "Rule has syntax error, but query OK\n");
    }

    Test->copy_all_logs(); return(Test->global_result);
}


