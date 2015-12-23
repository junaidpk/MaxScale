
/**
 * @file mariadb_nodes.cpp - backend nodes routines
 *
 * @verbatim
 * Revision History
 *
 * Date		Who		Description
 * 17/11/14	Timofey Turenko	Initial implementation
 *
 * @endverbatim
 */

#include "mariadb_nodes.h"
#include "sql_const.h"

Mariadb_nodes::Mariadb_nodes(char * pref)
{
    strcpy(prefix, pref);
    no_set_pos = false;
    verbose = true;
}

int Mariadb_nodes::connect()
{
    int res = 0;
    for (int i = 0; i < N; i++) {
        nodes[i] = open_conn(port[i], IP[i], user_name, password, ssl);
        if (mysql_errno(nodes[i]) != 0) {res++;}
    }
    return(res);
}

int Mariadb_nodes::close_connections()
{
    for (int i = 0; i < N; i++) {
        if (nodes[i] != NULL) {mysql_close(nodes[i]);}
    }
    return(0);
}

int Mariadb_nodes::read_env()
{
    char * env;
    char env_name[64];

    sprintf(env_name, "%s_N", prefix);
    env = getenv(env_name); if (env != NULL) {sscanf(env, "%d", &N); } else {N = 0;}

    sprintf(env_name, "%s_user", prefix);
    env = getenv(env_name); if (env != NULL) {sscanf(env, "%s", user_name); } else {sprintf(user_name, "skysql"); }
    sprintf(env_name, "%s_password", prefix);
    env = getenv(env_name); if (env != NULL) {sscanf(env, "%s", password); } else {sprintf(password, "skysql"); }

    ssl = false;
    sprintf(env_name, "%s_ssl", prefix);
    env = getenv(env_name); if ((env != NULL) && ((strcasecmp(env, "yes") == 0) || (strcasecmp(env, "true") == 0) )) {ssl = true;}


    if ((N > 0) && (N < 255)) {
        for (int i = 0; i < N; i++) {
            //reading IPs
            sprintf(env_name, "%s_%03d", prefix, i);
            env = getenv(env_name); if (env != NULL) {sprintf(IP[i], "%s", env);}

            //reading private IPs
            sprintf(env_name, "%s_private_%03d", prefix, i);
            env = getenv(env_name); if (env != NULL) {sprintf(IP_private[i], "%s", env);} else {sprintf(IP_private[i], "%s", IP[i]);}

            //reading ports
            sprintf(env_name, "%s_port_%03d", prefix, i);
            env = getenv(env_name); if (env != NULL) {
                sscanf(env, "%d", &port[i]);
            } else {
                port[i] = 3306;
            }
            //reading sshkey
            sprintf(env_name, "%s_sshkey_%03d", prefix, i);
            env = getenv(env_name); if (env != NULL) {sprintf(sshkey[i], "%s", env);}

            //reading start_db_command
            sprintf(env_name, "%s_start_db_command_%03d", prefix, i);
            env = getenv(env_name); if (env != NULL) {sprintf(start_db_command[i], "%s", env);} else {sprintf(start_db_command[i], "%s", "service mysql start");}

            //reading stop_db_command
            sprintf(env_name, "%s_stop_db_command_%03d", prefix, i);
            env = getenv(env_name); if (env != NULL) {sprintf(stop_db_command[i], "%s", env);} else {sprintf(start_db_command[i], "%s", "service mysql stop");}

            sprintf(env_name, "%s_kill_vm_command_%03d", prefix, i);
            env = getenv(env_name); if (env != NULL) {sscanf(env, "%s", kill_vm_command[i]); } else {sprintf(kill_vm_command[i], "exit 1"); }

            sprintf(env_name, "%s_start_vm_command_%03d", prefix, i);
            env = getenv(env_name); if (env != NULL) {sscanf(env, "%s", start_vm_command[i]); } else {sprintf(start_vm_command[i], "exit 1"); }

            sprintf(env_name, "%s_access_user_%03d", prefix, i);
            env = getenv(env_name); if (env != NULL) {sscanf(env, "%s", access_user[i]); } else {sprintf(access_user[i], "root"); }

            sprintf(env_name, "%s_access_sudo_%03d", prefix, i);
            env = getenv(env_name); if (env != NULL) {sscanf(env, "%s", access_sudo[i]); } else {sprintf(access_sudo[i], " "); }

            if (strcmp(access_user[i], "root") == 0) {
                sprintf(access_homedir[i], "/%s/", access_user[i]);
            } else {
                sprintf(access_homedir[i], "/home/%s/", access_user[i]);
            }

        }
    }
}

int Mariadb_nodes::print_env()
{
    for (int i = 0; i < N; i++)
    {
        printf("%s node %d \t%s\tPort=%d\n", prefix, i, IP[i], port[i]);
        printf("%s Access user %s\n", prefix, access_user[i]);
    }
    printf("%s User name %s\n", prefix, user_name);
    printf("%s Password %s\n", prefix, password);

}

int Mariadb_nodes::find_master()
{
    char str[255];
    char master_IP[256];
    int i = 0;
    int found = 0;
    int master_node = 255;
    while ((found == 0) && (i < N)) {
        if (find_field(
                    nodes[i], (char *) "show slave status;",
                    (char *) "Master_Host", &str[0]
                    ) == 0 ) {
            found = 1;
            strcpy(master_IP, str);
        }
        i++;
    }
    if (found == 1) {
        found = 0; i = 0;
        while ((found == 0) && (i < N)) {
            if (strcmp(IP[i], master_IP) == 0) {
                found = 1;
                master_node = i;
            }
            i++;
        }
    }
    return(master_node);
}

int Mariadb_nodes::change_master(int NewMaster, int OldMaster)
{
    int i;
    //int OldMaster = FindMaster();
    char log_file[256];
    char log_pos[256];
    char str[1024];

    for (i = 0; i < N; i++) {
        if (i != OldMaster) {execute_query(nodes[i], (char *) "stop slave;");}
    }
    execute_query(nodes[NewMaster], create_repl_user);
    execute_query(nodes[OldMaster], (char *) "reset master;");
    find_field(nodes[NewMaster], (char *) "show master status", (char *) "File", &log_file[0]);
    find_field(nodes[NewMaster], (char *) "show master status", (char *) "Position", &log_pos[0]);
    for (i = 0; i < N; i++) {
        if (i != NewMaster) {
            sprintf(str, setup_slave, IP[NewMaster], log_file, log_pos, port[NewMaster]);
            execute_query(nodes[i], str);
        }
    }
    //for (i = 0; i < N; i++) {if (i != NewMaster) {execute_query(nodes[i], (char *) "start slave;"); }}
}

int Mariadb_nodes::stop_node(int node)
{
    return(ssh_node(node, stop_db_command[node], true));
}

int Mariadb_nodes::start_node(int node, char * param)
{
    char cmd[1024];
    sprintf(cmd, "%s %s", start_db_command[node], param);
    return(ssh_node(node, cmd, true));
}

int Mariadb_nodes::stop_nodes()
{
    int i;
    int local_result = 0;
    char sys1[4096];
    connect();
    for (i = 0; i < N; i++)
    {
        printf("Stopping slave %d\n", i); fflush(stdout);
        local_result += execute_query(nodes[i], (char *) "stop slave;");
        printf("Stopping %d\n", i); fflush(stdout);
        local_result += stop_node(i); fflush(stdout);
    }
    return(local_result);
}

int Mariadb_nodes::stop_slaves()
{
    int i;
    int global_result = 0;
    connect();
    for (i = 0; i < N; i++) {
        printf("Stopping slave %d\n", i); fflush(stdout);
        global_result += execute_query(nodes[i], (char *) "stop slave;");
    }
    close_connections();
    return(global_result);
}

int Mariadb_nodes::start_replication()
{
    char sys1[4096];
    char str[1024];
    char log_file[256];
    char log_pos[256];
    int i;
    int global_result = 0;
    global_result += stop_nodes();

    printf("Starting back Master\n"); fflush(stdout);
    global_result += start_node(0, (char *) ""); fflush(stdout);

    for (i = 1; i < N; i++) {
        printf("Starting node %d\n", i); fflush(stdout);
        global_result += start_node(i, (char *) ""); fflush(stdout);
    }
    sleep(5);

    global_result += connect();
    global_result += execute_query(nodes[0], create_repl_user);
    execute_query(nodes[0], (char *) "reset master;");
    execute_query(nodes[0], (char *) "stop slave;");

    find_field(nodes[0], (char *) "show master status", (char *) "File", &log_file[0]);
    find_field(nodes[0], (char *) "show master status", (char *) "Position", &log_pos[0]);
    for (i = 1; i < N; i++) {
        global_result += execute_query(nodes[i], (char *) "stop slave;");
        sprintf(str, setup_slave, IP_private[0], log_file, log_pos, port[0]);
        if (this->verbose)
        {
            printf("%s", str);
        }
        global_result += execute_query(nodes[i], str);
    }
    close_connections();
    return(global_result);
}

int Mariadb_nodes::start_galera()
{
    char sys1[4096];
    int i;
    int global_result = 0;
    global_result += stop_nodes();

    printf("Starting new Galera cluster\n");  fflush(stdout);
    global_result += start_node(0, (char *) " --wsrep-cluster-address=gcomm://");

    for (i = 1; i < N; i++) {
        printf("Starting node %d\n", i); fflush(stdout);
        sprintf(&sys1[0], " --wsrep-cluster-address=gcomm://%s'", IP_private[0]);
        if (this->verbose)
        {
            printf("%s\n", sys1);
            fflush(stdout);
        }
        global_result += start_node(i, sys1); fflush(stdout);
    }
    sleep(5);

    global_result += connect();
    global_result += execute_query(nodes[0], create_repl_user);

    close_connections();
    return(global_result);
}

int Mariadb_nodes::block_node(int node)
{
    char sys1[1024];
    sprintf(&sys1[0], "iptables -I INPUT -p tcp --dport %d -j REJECT", port[node]);
    if (this->verbose)
    {
        printf("%s\n", sys1);
        fflush(stdout);
    }
    return(ssh_node(node, sys1, TRUE));
}

int Mariadb_nodes::unblock_node(int node)
{
    char sys1[1024];
    sprintf(&sys1[0], "iptables -I INPUT -p tcp --dport %d -j ACCEPT", port[node]);
    if (this->verbose)
    {
        printf("%s\n", sys1);
        fflush(stdout);
    }
    return(ssh_node(node, sys1, TRUE));
}


int Mariadb_nodes::unblock_all_nodes()
{
    int rval = 0;
    for (int i = 0; i < this->N; i++)
    {
        rval += this->unblock_node(i);
    }
    return rval;
}

int Mariadb_nodes::check_and_restart_nodes_vm()
{
    int res = 0;
    for (int i = 0; i < N; i++)
    {
        res += check_and_restart_node_vm(i);
    }
    return(res);
}

int Mariadb_nodes::check_node_vm(int node)
{
    int res = 0;
    printf("Checking node %d\n", node); fflush(stdout);

    if (ssh_node(0, (char *) "ls > /dev/null", false) != 0) {
        printf("Node %d is not available\n", node); fflush(stdout);
        res = 1;
    } else {
        printf("Node %d is OK\n", node); fflush(stdout);
    }
    return(res);
}

int Mariadb_nodes::restart_node_vm(int node)
{
    int res = 0;
    printf("stopping node %d: %s\n", node, kill_vm_command[node]);
    system(kill_vm_command[node]);
    printf("starting node %d: %s\n", node, start_vm_command[node]);
    res += system(start_vm_command[node]);
    return(res);
}

int Mariadb_nodes::check_and_restart_node_vm(int node)
{
    if (check_node_vm(node) != 0) {return(restart_node_vm(node));} else {return(0);}
}

int Mariadb_nodes::check_replication(int master)
{
    int res1 = 0;
    char str[1024];
    MYSQL *conn;
    MYSQL_RES *res;
    printf("Checking Master/Slave setup\n"); fflush(stdout);
    for (int i = 0; i < N; i++) {
        conn = open_conn(port[i], IP[i], user_name, password, ssl);
        if (mysql_errno(conn) != 0) {
            printf("Error connectiong node %d\n", i); fflush(stdout);
            res1 = 1;
        } else {
            if ( i == master ) {
                // checking master
                if (conn != NULL ) {
                    if(mysql_query(conn, (char *) "SHOW SLAVE HOSTS;") != 0) {
                        printf("%s\n", mysql_error(conn));
                        res1 = 1;
                    } else {
                        res = mysql_store_result(conn);
                        if(res == NULL) {
                            printf("Error: can't get the result description\n"); fflush(stdout);
                            res1 = 1;
                        } else {
                            if (mysql_num_rows(res) != N-1) {
                                printf("Number if slaves is not equal to N-1\n"); fflush(stdout);
                                res1 = 1;
                            }
                        }
                        mysql_free_result(res);
                        do {
                            res = mysql_store_result(conn);
                            mysql_free_result(res);
                        } while ( mysql_next_result(conn) == 0 );
                    }
                }

            } else {
                // checking slave
                if (find_field(conn, (char *) "SHOW SLAVE STATUS;", (char *) "Slave_IO_Running", str) != 0) {
                    printf("Slave_IO_Running is not found in SHOW SLAVE STATUS results\n"); fflush(stdout);
                    res1 = 1;
                } else {
                    if (strcmp(str, "Yes") !=0 ) {
                        printf("Slave_IO_Running is not Yes\n"); fflush(stdout);
                        res1 = 1;
                    }
                }
            }
        }
        mysql_close(conn);
    }
    return(res1);
}

int Mariadb_nodes::check_galera()
{
    int res1 = 0;
    char str[1024];
    int cluster_size;
    MYSQL *conn;
    printf("Checking Galera\n"); fflush(stdout);
    for (int i = 0; i < N; i++) {
        conn = open_conn(port[i], IP[i], user_name, password, ssl);
        if (mysql_errno(conn) != 0) {
            printf("Error connectiong node %d\n", i);
            res1 = 1;
        } else {
            if (find_field(conn, (char *) "SHOW STATUS WHERE Variable_name='wsrep_cluster_size';", (char *) "Value", str) != 0) {
                printf("wsrep_cluster_size is not found in SHOW STATUS LIKE 'wsrep%%' results\n"); fflush(stdout);
                res1 = 1;
            } else {
                sscanf(str, "%d",  &cluster_size);
                if (cluster_size != N ) {
                    printf("wsrep_cluster_size is not %d\n", N); fflush(stdout);
                    res1 = 1;
                }
            }
        }
        mysql_close(conn);
    }
    return(res1);
}

int Mariadb_nodes::wait_all_vm()
{
    int i = 0;

    while ((check_and_restart_nodes_vm() != 0) && (i < 20)) {
        sleep(10);
    }
    return(check_and_restart_nodes_vm());
}

int Mariadb_nodes::kill_all_vm()
{
    int res = 0;
    char sys[1024];
    for (int i = 0; i < N; i++) {
        sprintf(sys, "%s", kill_vm_command[i]);
        if (system(sys) != 0) {res = 1;}
    }
    return(res);
}

int Mariadb_nodes::start_all_vm()
{
    int res = 0;
    char sys[1024];
    for (int i = 0; i < N; i++) {
        printf("starting node %d\n", i);
        sprintf(sys, "%s", start_vm_command[i]);
        if (system(sys) != 0) {res = 1;}
    }
    return(res);
}

int Mariadb_nodes::restart_all_vm()
{
    kill_all_vm();
    start_all_vm();
    return(wait_all_vm());
}

int Mariadb_nodes::set_slave(MYSQL * conn, char master_host[], int master_port, char log_file[], char log_pos[])
{
    char str[1024];

    sprintf(str, setup_slave, master_host, log_file, log_pos, master_port);
    if (no_set_pos) {sprintf(str, setup_slave_no_pos, master_host, master_port);}

    if (this->verbose)
    {
        printf("Setup slave SQL: %s\n", str);
    }
    return(execute_query(conn, str));
}

int Mariadb_nodes::set_repl_user()
{
    int global_result = 0;
    global_result += connect();
    for (int i = 0; i < N; i++) {
        global_result += execute_query(nodes[i], create_repl_user);
    }
    close_connections();
    return(global_result);
}

int Mariadb_nodes::get_server_id(int index)
{
    int id = -1;
    char str[1024];

    if (find_field(this->nodes[index], "SELECT @@server_id", "@@server_id", (char*) str) == 0)
    {
        id = atoi(str);
    }
    else
    {
        printf("find_field failed for %s:%d\n", this->IP[index], this->port[index]);
    }

    return id;
}

void Mariadb_nodes::generate_ssh_cmd(char * cmd, int node, char * ssh, bool sudo)
{
    if (sudo)
    {
        sprintf(cmd, "ssh -i %s -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no  -o LogLevel=quiet %s@%s \"%s %s\"",
                sshkey[node], access_user[node], IP[node], access_sudo[node], ssh);
    } else
    {
        sprintf(cmd, "ssh -i %s -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no  -o LogLevel=quiet %s@%s \"%s\"",
                sshkey[node], access_user[node], IP[node], ssh);
    }
}

char * Mariadb_nodes::ssh_node_output(int node, char * ssh, bool sudo)
{
    char sys[strlen(ssh) + 1024];
    generate_ssh_cmd(sys, node, ssh, sudo);

    FILE *output = popen(sys, "r");
    char buffer[1024];
    size_t rsize = sizeof(buffer);
    char* result = (char*)calloc(rsize, sizeof(char));

    while(fgets(buffer, sizeof(buffer), output))
    {
        result = (char*)realloc(result, sizeof(buffer) + rsize);
        strcat(result, buffer);
    }

    return result;
}

int Mariadb_nodes::ssh_node(int node, char * ssh, bool sudo)
{
    char sys[strlen(ssh) + 1024];
    generate_ssh_cmd(sys, node, ssh, sudo);
    return(system(sys));
}

int Mariadb_nodes::flush_hosts()
{
    int local_result = 0;
    for (int i = 0; i < N; i++)
    {
        local_result += ssh_node(i, (char *) "mysqladmin flush-hosts", true);
    }
}
