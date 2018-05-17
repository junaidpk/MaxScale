#pragma once
/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2020-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include <maxscale/cppdefs.hh>
#include <maxscale/monitor.hh>
#include <maxscale/thread.h>

/**
 * @file mmmon.hh - The Multi-Master monitor
 */

class MMMonitor : public maxscale::MonitorInstance
{
public:
    MMMonitor(const MMMonitor&) = delete;
    MMMonitor& operator = (const MMMonitor&) = delete;

    static MMMonitor* create(MXS_MONITOR* monitor);
    void destroy();
    void diagnostics(DCB* dcb) const;
    json_t* diagnostics_json() const;

private:
    unsigned long m_id;      /**< Monitor ID */
    int m_detectStaleMaster; /**< Monitor flag for Stale Master detection */

    MMMonitor(MXS_MONITOR* monitor);
    ~MMMonitor();

    MXS_MONITORED_SERVER *get_current_master();

    bool has_sufficient_permissions() const;
    void configure(const MXS_CONFIG_PARAMETER* params);
    void tick();

    void main();
};