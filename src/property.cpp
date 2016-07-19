#include "property.hpp"
#include "task.hpp"
#include "config.hpp"
#include "cgroup.hpp"
#include "container.hpp"
#include "network.hpp"
#include "util/log.hpp"
#include "util/string.hpp"
#include "util/unix.hpp"
#include "util/cred.hpp"
#include <sstream>
#include "statistics.hpp"

extern "C" {
#include <linux/capability.h>
#include <sys/sysinfo.h>
}

extern __thread TContainer *CurrentContainer;
extern __thread TClient *CurrentClient;
TContainerUser ContainerUser(P_USER, USER_SET, "Start command with given user");
TContainerGroup ContainerGroup(P_GROUP, GROUP_SET, "Start command with given group");
TContainerMemoryGuarantee ContainerMemoryGuarantee(P_MEM_GUARANTEE, MEM_GUARANTEE_SET,
                                                    "Guaranteed amount of memory "
                                                    "[bytes] (dynamic)");
TContainerMemTotalGuarantee ContainerMemTotalGuarantee(P_MEM_TOTAL_GUARANTEE,
                                                       "Total amount of memory "
                                                       "guaranteed for porto "
                                                       "containers");
TContainerVirtMode ContainerVirtMode(P_VIRT_MODE, VIRT_MODE_SET,
                                     "Virtualization mode: os|app");
TContainerCommand ContainerCommand(P_COMMAND, COMMAND_SET,
                                   "Command executed upon container start");
TContainerCwd ContainerCwd(P_CWD, CWD_SET, "Container working directory");
TContainerStdinPath ContainerStdinPath(P_STDIN_PATH, STDIN_SET,
                                       "Container standard input path");
TContainerStdoutPath ContainerStdoutPath(P_STDOUT_PATH, STDOUT_SET,
                                         "Container standard output path");
TContainerStderrPath ContainerStderrPath(P_STDERR_PATH, STDERR_SET,
                                         "Container standard error path");
TContainerBindDns ContainerBindDns(P_BIND_DNS, BIND_DNS_SET,
                                   "Bind /etc/resolv.conf and /etc/hosts"
                                   " of host to container");
TContainerIsolate ContainerIsolate(P_ISOLATE, ISOLATE_SET,
                                   "Isolate container from parent");
TContainerRoot ContainerRoot(P_ROOT, ROOT_SET, "Container root directory "
                             "(container will be chrooted into this directory)");
TContainerRootRo ContainerRootRo(P_ROOT_RDONLY, ROOT_RDONLY_SET,
                                 "Mount root directory in read-only mode");
TContainerNet ContainerNet(P_NET, NET_SET,
                            "Container network settings: "
                            "none | "
                            "inherited | "
                            "host [interface] | "
                            "container <name> | "
                            "macvlan <master> <name> [bridge|private|vepa|passthru] "
                            "[mtu] [hw] | "
                            "ipvlan <master> <name> [l2|l3] [mtu] | "
                            "veth <name> <bridge> [mtu] [hw] | "
                            "L3 <name> [master] | "
                            "NAT [name] | "
                            "MTU <name> <mtu> | "
                            "autoconf <name> | "
                            "netns <name>");
TContainerHostname ContainerHostname(P_HOSTNAME, HOSTNAME_SET,
                                     "Container hostname");
TContainerEnv ContainerEnv(P_ENV, ENV_SET,
                           "Container environment variables: <name>=<value>; ...");
TContainerBind ContainerBind(P_BIND, BIND_SET,
                             "Share host directories with container: "
                             "<host_path> <container_path> [ro|rw]; ...");
TContainerIp ContainerIp(P_IP, IP_SET,
                         "IP configuration: <interface> <ip>/<prefix>; ...");
TContainerCapabilities ContainerCapabilities(P_CAPABILITIES, CAPABILITIES_SET,
                                             "Limit container capabilities: "
                                             "list of capabilities without CAP_"
                                             " prefix (man 7 capabilities)"
                                             );
TContainerDefaultGw ContainerDefaultGw(P_DEFAULT_GW, DEFAULT_GW_SET,
                                       "Default gateway: <interface> <ip>; ...");
TContainerResolvConf ContainerResolvConf(P_RESOLV_CONF, RESOLV_CONF_SET,
                                         "DNS resolver configuration: "
                                         "<resolv.conf option>;...");
TContainerDevices ContainerDevices(P_DEVICES, DEVICES_SET,
                                   "Devices that container can access: "
                                   "<device> [r][w][m][-] [name] [mode] "
                                   "[user] [group]; ...");
TContainerRawRootPid ContainerRawRootPid(P_RAW_ROOT_PID, "");
TContainerRawLoopDev ContainerRawLoopDev(P_RAW_LOOP_DEV, "");
TContainerRawStartTime ContainerRawStartTime(P_RAW_START_TIME, "");
TContainerRawDeathTime ContainerRawDeathTime(P_RAW_DEATH_TIME, "");
TContainerUlimit ContainerUlimit(P_ULIMIT, ULIMIT_SET,
                                 "Container resource limits: "
                                 "<type> <soft> <hard>; ... (man 2 getrlimit)");
TContainerPortoNamespace ContainerPortoNamespace(P_PORTO_NAMESPACE,
                                            PORTO_NAMESPACE_SET,
                                            "Porto containers namespace "
                                            "(container name prefix) (dynamic)");
TContainerStdoutLimit ContainerStdoutLimit(P_STDOUT_LIMIT, STDOUT_LIMIT_SET,
                                           "Limit returned stdout/stderr "
                                           "size (dynamic)");
TContainerMemoryLimit ContainerMemoryLimit(P_MEM_LIMIT, MEM_LIMIT_SET,
                                           "Memory hard limit [bytes] (dynamic)");
TContainerAnonLimit ContainerAnonLimit(P_ANON_LIMIT, ANON_LIMIT_SET,
                                       "Anonymous memory limit [bytes] (dynamic)");
TContainerDirtyLimit ContainerDirtyLimit(P_DIRTY_LIMIT, DIRTY_LIMIT_SET,
                                         "Dirty file cache limit [bytes] "
                                         "(dynamic)");
TContainerRechargeOnPgfault ContainerRechargeOnPgfault(P_RECHARGE_ON_PGFAULT,
                                                       RECHARGE_ON_PGFAULT_SET,
                                                       "Recharge memory on "
                                                       "page fault (dynamic)");
TContainerCpuPolicy ContainerCpuPolicy(P_CPU_POLICY, CPU_POLICY_SET,
                                       "CPU policy: rt, normal, idle (dynamic)");
TContainerCpuLimit ContainerCpuLimit(P_CPU_LIMIT, CPU_LIMIT_SET,
                                     "CPU limit: 0-100.0 [%] | 0.0c-<CPUS>c "
                                     " [cores] (dynamic)");
TContainerCpuGuarantee ContainerCpuGuarantee(P_CPU_GUARANTEE, CPU_GUARANTEE_SET,
                                             "CPU guarantee: 0-100.0 [%] | "
                                             "0.0c-<CPUS>c [cores] (dynamic)");
TContainerIoPolicy ContainerIoPolicy(P_IO_POLICY, IO_POLICY_SET,
                                     "IO policy: normal, batch (dynamic)");
TContainerIoLimit ContainerIoLimit(P_IO_LIMIT, IO_LIMIT_SET,
                                   "Filesystem bandwidth limit [bytes/s] "
                                   "(dynamic)");
TContainerIopsLimit ContainerIopsLimit(P_IO_OPS_LIMIT, IO_OPS_LIMIT_SET,
                                       "Filesystem IOPS limit "
                                       "[operations/s] (dynamic)");
TContainerNetGuarantee ContainerNetGuarantee(P_NET_GUARANTEE, NET_GUARANTEE_SET,
                                             "Guaranteed container network "
                                             "bandwidth: <interface>|default "
                                             "<Bps>;... (dynamic)");
TContainerNetLimit ContainerNetLimit(P_NET_LIMIT, NET_LIMIT_SET,
                                     "Maximum container network bandwidth: "
                                     "<interface>|default <Bps>;... (dynamic)");
TContainerNetPriority ContainerNetPriority(P_NET_PRIO, NET_PRIO_SET,
                                           "Container network priority: "
                                           "<interface>|default 0-7;... "
                                           "(dynamic)");
TContainerRespawn ContainerRespawn(P_RESPAWN, RESPAWN_SET,
                                   "Automatically respawn dead container "
                                   "(dynamic)");
TContainerMaxRespawns ContainerMaxRespawns(P_MAX_RESPAWNS, MAX_RESPAWNS_SET,
                                           "Limit respawn count for specific "
                                           "container (dynamic)");
TContainerPrivate ContainerPrivate(P_PRIVATE, PRIVATE_SET,
                                   "User-defined property (dynamic)");
TContainerNetTos ContainerNetTos(P_NET_TOS, NET_TOS_SET, "IP TOS");
TContainerAgingTime ContainerAgingTime(P_AGING_TIME, AGING_TIME_SET,
                                      "After given number of seconds "
                                      "container in dead state is "
                                      "automatically removed (dynamic)");
TContainerEnablePorto ContainerEnablePorto(P_ENABLE_PORTO, ENABLE_PORTO_SET,
                                           "Allow container communication "
                                           "with porto (dynamic)");
TContainerWeak ContainerWeak(P_WEAK, WEAK_SET, "Destroy container when "
                                               "client disconnects (dynamic)");
TContainerAbsoluteName ContainerAbsoluteName(D_ABSOLUTE_NAME,
                                             "container name including porto "
                                             "namespaces (ro)");
TContainerAbsoluteNamespace ContainerAbsoluteNamespace(D_ABSOLUTE_NAMESPACE,
                                                       "container namespace "
                                                       "including parent "
                                                       "namespaces (ro)");
TContainerState ContainerState(D_STATE, "container state (ro)");
TContainerOomKilled ContainerOomKilled(D_OOM_KILLED,
                                       "container has been killed by OOM (ro)");
TContainerParent ContainerParent(D_PARENT,
                                 "parent container name (ro) (deprecated)");
TContainerRespawnCount ContainerRespawnCount(D_RESPAWN_COUNT,
                                             "current respawn count (ro)");
TContainerRootPid ContainerRootPid(D_ROOT_PID, "root task pid (ro)");
TContainerExitStatus ContainerExitStatus(D_EXIT_STATUS,
                                         "container exit status (ro)");
TContainerStartErrno ContainerStartErrno(D_START_ERRNO,
                                         "container start error (ro)");
TContainerStdout ContainerStdout(D_STDOUT,
                                 "stdout (optional start [offset]) (ro)");
TContainerStdoutOffset ContainerStdoutOffset(D_STDOUT_OFFSET,
                                             "stored stdout offset (ro)");
TContainerStderr ContainerStderr(D_STDERR,
                                 "stderr (optional start [offset]) (ro)");
TContainerStderrOffset ContainerStderrOffset(D_STDERR_OFFSET,
                                             "stored stderr offset (ro)");
TContainerMemUsage ContainerMemUsage(D_MEMORY_USAGE,
                                     "current memory usage [bytes] (ro)");
TContainerAnonUsage ContainerAnonUsage(D_ANON_USAGE,
                                       "current anonymous memory usage "
                                       "[bytes] (ro)");
TContainerMinorFaults ContainerMinorFaults(D_MINOR_FAULTS,
                                           "minor page faults (ro)");
TContainerMajorFaults ContainerMajorFaults(D_MAJOR_FAULTS,
                                           "major page faults (ro)");
TContainerMaxRss ContainerMaxRss(D_MAX_RSS,
                                 "peak anonymous memory usage [bytes] (ro)");
TContainerCpuUsage ContainerCpuUsage(D_CPU_USAGE,
                                     "consumed CPU time [nanoseconds] (ro)");
TContainerCpuSystem ContainerCpuSystem(D_CPU_SYSTEM,
                                     "consumed system CPU time "
                                     "[nanoseconds] (ro)");
TContainerNetBytes ContainerNetBytes(D_NET_BYTES,
                                     "tx bytes: <interface>: "
                                     "<bytes>;... (ro)");
TContainerNetPackets ContainerNetPackets(D_NET_PACKETS,
                                         "tx packets: <interface>: "
                                         "<packets>;... (ro)");
TContainerNetDrops ContainerNetDrops(D_NET_DROPS,
                                     "dropped tx packets: <interface>: "
                                     "<packets>;... (ro)");
TContainerNetOverlimits ContainerNetOverlimits(D_NET_OVERLIMITS,
                                               "overlimit tx packets: "
                                               "<interface>: <packets>;"
                                               "... (ro)");
TContainerNetRxBytes ContainerNetRxBytes(D_NET_RX_BYTES,
                                         "rx bytes: <interface>: "
                                         "<bytes>;... (ro)");
TContainerNetRxPackets ContainerNetRxPackets(D_NET_RX_PACKETS,
                                             "rx packets: <interface>: "
                                             "<packets>;... (ro)");
TContainerNetRxDrops ContainerNetRxDrops(D_NET_RX_DROPS,
                                         "dropped rx packets: <interface>: "
                                         "<packets>;... (ro)");
TContainerIoRead ContainerIoRead(D_IO_READ, "read from disk [bytes] (ro)");
TContainerIoWrite ContainerIoWrite(D_IO_WRITE, "written to disk [bytes] (ro)");
TContainerIoOps ContainerIoOps(D_IO_OPS, "io operations (ro)");
TContainerTime ContainerTime(D_TIME, "running time [seconds] (ro)");
TContainerPortoStat ContainerPortoStat(D_PORTO_STAT, "porto statistics (ro)");
extern std::map<std::string, TContainerProperty*> ContainerPropMap;
extern TStatistics *Statistics;

/*
 * Note for other properties:
 * Dead state 2-line check is mandatory for all properties
 * Some properties require to check if the property is supported
 * Some properties may forbid changing it in runtime
 * Of course, some properties can be read-only
 */

TError TContainerProperty::IsAliveAndStopped(void) {
    auto state = CurrentContainer->GetState();

    if (state == EContainerState::Dead)
        return TError(EError::InvalidState,
                      "Cannot change property while in the dead state");

    if (state != EContainerState::Stopped &&
        state != EContainerState::Unknown)
        return TError(EError::InvalidState,
                "Cannot change property in runtime");

    return TError::Success();
}

TError TContainerProperty::IsAlive(void) {
    auto state = CurrentContainer->GetState();

    if (state == EContainerState::Dead)
        return TError(EError::InvalidState,
                      "Cannot change property while in the dead state");

    return TError::Success();
}

TError TContainerProperty::IsDead(void) {
    auto state = CurrentContainer->GetState();

    if (state != EContainerState::Dead)
        return TError(EError::InvalidState,
                      "Available only in dead state: " + Name);

    return TError::Success();
}

TError TContainerProperty::IsRunning(void) {
    auto state = CurrentContainer->GetState();

    /*
     * This snippet is taken from TContainer::GetProperty.
     * The method name misguides a bit, but may be the semantic
     * of such properties is that we can look at the value in
     * the dead state too...
     */
    if (state == EContainerState::Stopped)
        return TError(EError::InvalidState,
                      "Not available in stopped state: " + Name);

    return TError::Success();
}

class TContainerUser : public TContainerProperty {
public:
    TError Set(const std::string &username);
    TError Get(std::string &value);
    TContainerUser(std::string name, uint64_t set_mask, std::string desc)
                   : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerUser::Set(const std::string &username) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    uid_t new_uid;
    error = UserId(username, new_uid);
    if (error)
        return error;

    TCred &owner = CurrentContainer->OwnerCred;
    TCred new_user(new_uid, owner.Gid);
    error = FindGroups(username, new_user.Gid, new_user.Groups);
    if (error)
        return error;

    TCred &cur_user = CurrentClient->Cred;

    if (cur_user.CanControl(new_user)) {
        owner.Uid = new_user.Uid;
        owner.Groups.clear();
        owner.Groups.insert(owner.Groups.end(), new_user.Groups.begin(),
                            new_user.Groups.end());

        if (!(CurrentContainer->PropMask & CAPABILITIES_SET)) {
            if (owner.IsRootUser())
                CurrentContainer->Caps = ContainerCapabilities.AllCaps;
            else if (CurrentContainer->VirtMode == VIRT_MODE_OS)
                CurrentContainer->Caps = PermittedCaps;
            else
                CurrentContainer->Caps = 0;
        }

        CurrentContainer->PropMask |= USER_SET;

        return TError::Success();
    }

    return TError(EError::Permission,
                  "Client is not allowed to set user : " + username);
}

TError TContainerUser::Get(std::string &value) {
    value = UserName(CurrentContainer->OwnerCred.Uid);

    return TError::Success();
}

class TContainerGroup : public TContainerProperty {
public:
    TError Set(const std::string &groupname);
    TError Get(std::string &value);
    TContainerGroup(std::string name, uint64_t set_mask, std::string desc)
                    : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerGroup::Set(const std::string &groupname) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    gid_t new_gid;
    error = GroupId(groupname, new_gid);
    if (error)
        return error;

    if (CurrentClient->Cred.IsRootUser()) {
        CurrentContainer->OwnerCred.Gid = new_gid;
        CurrentContainer->PropMask |= GROUP_SET;

        return TError::Success();
    }

    if (CurrentContainer->OwnerCred.IsMemberOf(new_gid)) {
        CurrentContainer->OwnerCred.Gid = new_gid;
        CurrentContainer->PropMask |= GROUP_SET;

        return TError::Success();
    }

    return TError(EError::Permission, "Desired group : " + groupname +
                  " isn't in current user supplementary group list");
}
    TError SetValue(const std::string &value);

TError TContainerGroup::Get(std::string &value) {
    value = GroupName(CurrentContainer->OwnerCred.Gid);

    return TError::Success();
}

class TContainerMemoryGuarantee : public TContainerProperty {
public:
    TError Set(const std::string &mem_guarantee);
    TError Get(std::string &value);
    TContainerMemoryGuarantee(std::string name, uint64_t set_mask,
                              std::string desc)
                              : TContainerProperty(name, set_mask, desc) {}
    TError Init(void) {
        IsSupported = MemorySubsystem.SupportGuarantee();

        return TError::Success();
    }
};

TError TContainerMemoryGuarantee::Set(const std::string &mem_guarantee) {
    TError error = IsAlive();
    if (error)
        return error;

    uint64_t new_val = 0lu;
    error = StringToSize(mem_guarantee, new_val);
    if (error)
        return error;

    CurrentContainer->CurrentMemGuarantee = new_val;

    uint64_t usage = CurrentContainer->GetRoot()->GetHierarchyMemGuarantee();
    uint64_t total = GetTotalMemory();
    uint64_t reserve = config().daemon().memory_guarantee_reserve();
    if (usage + reserve > total) {
        CurrentContainer->CurrentMemGuarantee = CurrentContainer->MemGuarantee;

        return TError(EError::ResourceNotAvailable,
                "can't guarantee all available memory: requested " +
                std::to_string(new_val) + " (will be " + std::to_string(usage) +
                " of " + std::to_string(total) + ", reserve " + std::to_string(reserve) + ")");
    }

    if (CurrentContainer->GetState() == EContainerState::Running ||
        CurrentContainer->GetState() == EContainerState::Meta ||
        CurrentContainer->GetState() == EContainerState::Paused) {
        auto memcg = CurrentContainer->GetCgroup(MemorySubsystem);
        error = MemorySubsystem.SetGuarantee(memcg, new_val);

        if (error) {
            CurrentContainer->CurrentMemGuarantee = CurrentContainer->MemGuarantee;
            L_ERR() << "Can't set " << P_MEM_GUARANTEE << ": " << error << std::endl;

            return error;
        }
    }

    CurrentContainer->MemGuarantee = new_val;
    CurrentContainer->PropMask |= MEM_GUARANTEE_SET;

    return TError::Success();
}

TError TContainerMemoryGuarantee::Get(std::string &value) {
    value = std::to_string(CurrentContainer->MemGuarantee);

    return TError::Success();
}

class TContainerMemTotalGuarantee : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerMemTotalGuarantee(std::string name, std::string desc)
                                : TContainerProperty(name, desc) {}
    TError Init(void) {
        IsSupported = MemorySubsystem.SupportGuarantee();

        return TError::Success();
    }
};

TError TContainerMemTotalGuarantee::Get(std::string &value) {
    uint64_t total = CurrentContainer->GetHierarchyMemGuarantee();
    value = std::to_string(total);

    return TError::Success();
}

class TContainerCommand : public TContainerProperty {
public:
    TError Set(const std::string &command);
    TError Get(std::string &value);
    TContainerCommand(std::string name, uint64_t set_mask, std::string desc)
                      : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerCommand::Set(const std::string &command) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    CurrentContainer->Command = command;
    CurrentContainer->PropMask |= COMMAND_SET;

    return TError::Success();
}

TError TContainerCommand::Get(std::string &value) {
    std::string virt_mode;

    value = CurrentContainer->Command;

    return TError::Success();
}

class TContainerVirtMode : public TContainerProperty {
public:
    TError Set(const std::string &virt_mode);
    TError Get(std::string &value);
    TContainerVirtMode(std::string name, uint64_t set_mask, std::string desc)
                       : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerVirtMode::Set(const std::string &virt_mode) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    if (virt_mode == P_VIRT_MODE_APP)
        CurrentContainer->VirtMode = VIRT_MODE_APP;

    else if (virt_mode == P_VIRT_MODE_OS)
        CurrentContainer->VirtMode = VIRT_MODE_OS;

    else
        return TError(EError::InvalidValue, std::string("Unsupported ") +
                      P_VIRT_MODE + ": " + virt_mode);

    if (CurrentContainer->VirtMode == VIRT_MODE_OS) {
        if (!(CurrentContainer->PropMask & CWD_SET)) {

            CurrentContainer->Cwd = "/";
            ContainerCwd.Propagate("/");
        }

        if (!(CurrentContainer->PropMask & COMMAND_SET))
            CurrentContainer->Command = "/sbin/init";

        if (!(CurrentContainer->PropMask & STDOUT_SET))
            CurrentContainer->StdoutPath = "/dev/null";

        if (!(CurrentContainer->PropMask & STDERR_SET))
            CurrentContainer->StderrPath = "/dev/null";

        if (!(CurrentContainer->PropMask & BIND_DNS_SET))
            CurrentContainer->BindDns = false;

        if (!(CurrentContainer->PropMask & NET_SET))
            CurrentContainer->NetProp = { "none" };

        if (!(CurrentContainer->PropMask & CAPABILITIES_SET))
            CurrentContainer->Caps = PermittedCaps;
    }

    CurrentContainer->PropMask |= VIRT_MODE_SET;

    return TError::Success();
}

TError TContainerVirtMode::Get(std::string &value) {

    switch (CurrentContainer->VirtMode) {
        case VIRT_MODE_APP:
            value = P_VIRT_MODE_APP;
            break;
        case VIRT_MODE_OS:
            value = P_VIRT_MODE_OS;
            break;
        default:
            value = "unknown " + std::to_string(CurrentContainer->VirtMode);
            break;
    }

    return TError::Success();
}

class TContainerStdinPath : public TContainerProperty {
public:
    TError Set(const std::string &path);
    TError Get(std::string &value);
    TContainerStdinPath(std::string name, uint64_t set_mask, std::string desc)
                    : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerStdinPath::Set(const std::string &path) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    CurrentContainer->StdinPath = path;
    CurrentContainer->PropMask |= STDIN_SET;

    return TError::Success();
}

TError TContainerStdinPath::Get(std::string &value) {
    value = CurrentContainer->StdinPath;

    return TError::Success();
}

class TContainerStdoutPath : public TContainerProperty {
public:
    TError Set(const std::string &path);
    TError Get(std::string &value);
    TContainerStdoutPath(std::string name, uint64_t set_mask, std::string desc)
                     : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerStdoutPath::Set(const std::string &path) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    CurrentContainer->StdoutPath = path;
    CurrentContainer->PropMask |= STDOUT_SET;

    return TError::Success();
}

TError TContainerStdoutPath::Get(std::string &value) {
    value = CurrentContainer->StdoutPath;

    return TError::Success();
}

class TContainerStderrPath : public TContainerProperty {
public:
    TError Set(const std::string &path);
    TError Get(std::string &value);
    TContainerStderrPath(std::string name, uint64_t set_mask, std::string desc)
                     : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerStderrPath::Set(const std::string &path) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    CurrentContainer->StderrPath = path;
    CurrentContainer->PropMask |= STDERR_SET;

    return TError::Success();
}

TError TContainerStderrPath::Get(std::string &value) {
    value = CurrentContainer->StderrPath;

    return TError::Success();
}

class TContainerBindDns : public TContainerProperty {
public:
    TError Set(const std::string &bind_needed);
    TError Get(std::string &value);
    TContainerBindDns(std::string name, uint64_t set_mask, std::string desc)
                    : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerBindDns::Get(std::string &value) {
    value = CurrentContainer->BindDns ? "true" : "false";

    return TError::Success();
}

TError TContainerBindDns::Set(const std::string &bind_needed) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    if (bind_needed == "true")
        CurrentContainer->BindDns = true;
    else if (bind_needed == "false")
        CurrentContainer->BindDns = false;
    else
        return TError(EError::InvalidValue, "Invalid bool value");

    CurrentContainer->PropMask |= BIND_DNS_SET;

    return TError::Success();
}

class TContainerIsolate : public TContainerProperty {
public:
    TError Set(const std::string &isolate_needed);
    TError Get(std::string &value);
    TContainerIsolate(std::string name, uint64_t set_mask, std::string desc)
                      : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerIsolate::Get(std::string &value) {
    value = CurrentContainer->Isolate ? "true" : "false";

    return TError::Success();
}

TError TContainerIsolate::Set(const std::string &isolate_needed) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    if (isolate_needed == "true")
        CurrentContainer->Isolate = true;
    else if (isolate_needed == "false")
        CurrentContainer->Isolate = false;
    else
        return TError(EError::InvalidValue, "Invalid bool value");

    if (!CurrentContainer->Isolate) {
        if (!(CurrentContainer->PropMask & BIND_DNS_SET))
            CurrentContainer->BindDns = false;

        auto p = CurrentContainer->GetParent();
        if (p) {
            if (!(CurrentContainer->PropMask & CWD_SET)) {
                CurrentContainer->Cwd = p->Cwd;

                ContainerCwd.Propagate(p->Cwd);
            }
            if (!(CurrentContainer->PropMask & ULIMIT_SET)) {
               CurrentContainer->Rlimit = p->Rlimit;

               ContainerUlimit.Propagate(CurrentContainer->Rlimit);
            }
            if (!(CurrentContainer->PropMask & CPU_POLICY_SET)) {
                CurrentContainer->CpuPolicy = p->CpuPolicy;

                ContainerCpuPolicy.Propagate(CurrentContainer->CpuPolicy);
            }
            if (!(CurrentContainer->PropMask & IO_POLICY_SET)) {
                CurrentContainer->IoPolicy = p->IoPolicy;

                ContainerIoPolicy.Propagate(CurrentContainer->IoPolicy);
            }
        }

    }

    CurrentContainer->PropMask |= ISOLATE_SET;

    return TError::Success();
}

class TContainerRoot : public TContainerProperty {
public:
    TError Set(const std::string &root);
    TError Get(std::string &value);
    TContainerRoot(std::string name, uint64_t set_mask, std::string desc)
                   : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerRoot::Get(std::string &value) {
    value = CurrentContainer->Root;

    return TError::Success();
}

TError TContainerRoot::Set(const std::string &root) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    CurrentContainer->Root = root;

    if (root != "/") {
        if (!(CurrentContainer->PropMask & BIND_DNS_SET) &&
            CurrentContainer->VirtMode != VIRT_MODE_OS &&
            CurrentContainer->Isolate) {
            CurrentContainer->BindDns = true;
        }

        if (!(CurrentContainer->PropMask & CWD_SET)) {
            CurrentContainer->Cwd = "/";
            ContainerCwd.Propagate("/");
        }
    }
    CurrentContainer->PropMask |= ROOT_SET;

    return TError::Success();
}

class TContainerNet : public TContainerProperty {
public:
    TError Set(const std::string &net_desc);
    TError Get(std::string &value);
    TContainerNet(std::string name, uint64_t set_mask, std::string desc)
                  : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerNet::Set(const std::string &net_desc) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    std::vector<std::string> new_net_desc;
    error = StringToStrList(net_desc, new_net_desc);

    TNetCfg cfg;
    error = cfg.ParseNet(new_net_desc);
    if (error)
        return error;

    CurrentContainer->NetProp = new_net_desc; /* FIXME: Copy vector contents? */

    CurrentContainer->PropMask |= NET_SET;
    return TError::Success();
}

TError TContainerNet::Get(std::string &value) {
    return StrListToString(CurrentContainer->NetProp, value);
}

class TContainerCwd : public TContainerProperty {
public:
    TError Set(const std::string &cwd);
    TError Get(std::string &value);
    TContainerCwd(std::string name, uint64_t set_mask, std::string desc)
                  : TContainerProperty(name, set_mask, desc) {}
    void Propagate(const std::string &value);
};

TError TContainerCwd::Set(const std::string &cwd) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    CurrentContainer->Cwd = cwd;
    Propagate(cwd);
    CurrentContainer->PropMask |= CWD_SET;

    return TError::Success();
}

TError TContainerCwd::Get(std::string &value) {
    value = CurrentContainer->Cwd;

    return TError::Success();
}

void TContainerCwd::Propagate(const std::string &cwd) {

    for (auto iter : CurrentContainer->Children) {
        if (auto child = iter.lock()) {

            auto old = CurrentContainer;
            CurrentContainer = child.get();

            if (!(CurrentContainer->PropMask & CWD_SET) &&
                !(CurrentContainer->Isolate)) {

                CurrentContainer->Cwd = cwd;
                ContainerCwd.Propagate(cwd);
            }
            CurrentContainer = old;
        }
    }
}

class TContainerRootRo : public TContainerProperty {
public:
    TError Set(const std::string &ro);
    TError Get(std::string &value);
    TContainerRootRo(std::string name, uint64_t set_mask, std::string desc)
                     : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerRootRo::Set(const std::string &ro) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    if (ro == "true")
        CurrentContainer->RootRo = true;
    else if (ro == "false")
        CurrentContainer->RootRo = false;
    else
        return TError(EError::InvalidValue, "Invalid bool value");
    
    CurrentContainer->PropMask |= ROOT_RDONLY_SET;

    return TError::Success();
}

TError TContainerRootRo::Get(std::string &ro) {
    ro = CurrentContainer->RootRo ? "true" : "false";

    return TError::Success();
}

class TContainerHostname : public TContainerProperty {
public:
    TError Set(const std::string &hostname);
    TError Get(std::string &value);
    TContainerHostname(std::string name, uint64_t set_mask, std::string desc)
                       : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerHostname::Set(const std::string &hostname) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    CurrentContainer->Hostname = hostname;
    CurrentContainer->PropMask |= HOSTNAME_SET;

    return TError::Success();
}

TError TContainerHostname::Get(std::string &value) {
    value = CurrentContainer->Hostname;

    return TError::Success();
}

class TContainerEnv : public TContainerProperty {
public:
    TError Set(const std::string &env);
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TError SetIndexed(const std::string &index, const std::string &env_val);
    TContainerEnv(std::string name, uint64_t set_mask, std::string desc)
                  : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerEnv::Set(const std::string &env_val) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    std::vector<std::string> envs;

    error = StringToStrList(env_val, envs);
    if (error)
        return error;

    TEnv env;
    error =  env.Parse(envs, true);
    if (error)
        return error;

    env.Format(CurrentContainer->EnvCfg);
    CurrentContainer->PropMask |= ENV_SET;

    return TError::Success();
}

TError TContainerEnv::Get(std::string &value) {
    return StrListToString(CurrentContainer->EnvCfg, value);
}

TError TContainerEnv::SetIndexed(const std::string &index, const std::string &env_val) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    TEnv env;
    error = env.Parse(CurrentContainer->EnvCfg, true);
    if (error)
        return error;

    error = env.Parse({index + "=" + env_val}, true);
    if (error)
        return error;

    env.Format(CurrentContainer->EnvCfg);
    CurrentContainer->PropMask |= ENV_SET;

    return TError::Success();
}

TError TContainerEnv::GetIndexed(const std::string &index, std::string &value) {
    TEnv env;
    TError error = CurrentContainer->GetEnvironment(env);
    if (error)
        return error;

    if (!env.GetEnv(index, value))
        return TError(EError::InvalidValue, "Variable " + index + " not defined");

    return TError::Success();
}

class TContainerBind : public TContainerProperty {
public:
    TError Set(const std::string &bind_str);
    TError Get(std::string &value);
    TContainerBind(std::string name, uint64_t set_mask, std::string desc)
                   : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerBind::Set(const std::string &bind_str) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    std::vector<std::string> binds;
    error = StringToStrList(bind_str, binds);
    if (error)
        return error;

    std::vector<TBindMap> bm;

    for (auto &line : binds) {
        std::vector<std::string> tok;
        TBindMap m;

        TError error = SplitEscapedString(line, ' ', tok);
        if (error)
            return error;

        if (tok.size() != 2 && tok.size() != 3)
            return TError(EError::InvalidValue, "Invalid bind in: " + line);

        m.Source = tok[0];
        m.Dest = tok[1];
        m.Rdonly = false;

        if (tok.size() == 3) {
            if (tok[2] == "ro")
                m.Rdonly = true;
            else if (tok[2] == "rw")
                m.Rdonly = false;
            else
                return TError(EError::InvalidValue, "Invalid bind type in: " + line);
        }

        bm.push_back(m);

    }

    CurrentContainer->BindMap = bm;
    CurrentContainer->PropMask |= BIND_SET;

    return TError::Success();
}

TError TContainerBind::Get(std::string &value) {
    std::vector<std::string> bind_str_list;
    for (auto m : CurrentContainer->BindMap)
        bind_str_list.push_back(m.Source.ToString() + " " + m.Dest.ToString() + 
                                (m.Rdonly ? " ro" : " rw"));
    
    return StrListToString(bind_str_list, value);
}

class TContainerIp : public TContainerProperty {
public:
    TError Set(const std::string &ipaddr);
    TError Get(std::string &value);
    TContainerIp(std::string name, uint64_t set_mask, std::string desc)
                 : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerIp::Set(const std::string &ipaddr) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    std::vector<std::string> ipaddrs;
    error = StringToStrList(ipaddr, ipaddrs);
    if (error)
        return error;

    TNetCfg cfg;
    error = cfg.ParseIp(ipaddrs);
    if (error)
        return error;

    CurrentContainer->IpList = ipaddrs;
    CurrentContainer->PropMask |= IP_SET;

    return TError::Success();
}

TError TContainerIp::Get(std::string &value) {
    return StrListToString(CurrentContainer->IpList, value);
}

class TContainerCapabilities : public TContainerProperty {
public:
    const std::map<std::string, uint64_t> SupportedCaps = {
        { "AUDIT_READ",         CAP_AUDIT_READ },
        { "CHOWN",              CAP_CHOWN },
        { "DAC_OVERRIDE",       CAP_DAC_OVERRIDE },
        { "DAC_READ_SEARCH",    CAP_DAC_READ_SEARCH },
        { "FOWNER",             CAP_FOWNER },
        { "FSETID",             CAP_FSETID },
        { "KILL",               CAP_KILL },
        { "SETGID",             CAP_SETGID },
        { "SETUID",             CAP_SETUID },
        { "SETPCAP",            CAP_SETPCAP },
        { "LINUX_IMMUTABLE",    CAP_LINUX_IMMUTABLE },
        { "NET_BIND_SERVICE",   CAP_NET_BIND_SERVICE },
        { "NET_BROADCAST",      CAP_NET_BROADCAST },
        { "NET_ADMIN",          CAP_NET_ADMIN },
        { "NET_RAW",            CAP_NET_RAW },
        { "IPC_LOCK",           CAP_IPC_LOCK },
        { "IPC_OWNER",          CAP_IPC_OWNER },
        { "SYS_MODULE",         CAP_SYS_MODULE },
        { "SYS_RAWIO",          CAP_SYS_RAWIO },
        { "SYS_CHROOT",         CAP_SYS_CHROOT },
        { "SYS_PTRACE",         CAP_SYS_PTRACE },
        { "SYS_PACCT",          CAP_SYS_PACCT },
        { "SYS_ADMIN",          CAP_SYS_ADMIN },
        { "SYS_BOOT",           CAP_SYS_BOOT },
        { "SYS_NICE",           CAP_SYS_NICE },
        { "SYS_RESOURCE",       CAP_SYS_RESOURCE },
        { "SYS_TIME",           CAP_SYS_TIME },
        { "SYS_TTY_CONFIG",     CAP_SYS_TTY_CONFIG },
        { "MKNOD",              CAP_MKNOD },
        { "LEASE",              CAP_LEASE },
        { "AUDIT_WRITE",        CAP_AUDIT_WRITE },
        { "AUDIT_CONTROL",      CAP_AUDIT_CONTROL },
        { "SETFCAP",            CAP_SETFCAP },
        { "MAC_OVERRIDE",       CAP_MAC_OVERRIDE },
        { "MAC_ADMIN",          CAP_MAC_ADMIN },
        { "SYSLOG",             CAP_SYSLOG },
        { "WAKE_ALARM",         CAP_WAKE_ALARM },
        { "BLOCK_SUSPEND",      CAP_BLOCK_SUSPEND },
    };
    uint64_t AllCaps;
    TError Init(void) {
        AllCaps = 0xffffffffffffffff >> (63 - LastCapability);

        return TError::Success();
    }
    TError Set(const std::string &caps);
    TError Get(std::string &value);
    TContainerCapabilities(std::string name, uint64_t set_mask, std::string desc)
                           : TContainerProperty(name, set_mask, desc, true) {}
};

TError TContainerCapabilities::Set(const std::string &caps_str) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    if (!CurrentClient->Cred.IsRootUser())
        return TError(EError::Permission, "Only root can change this property");

    std::vector<std::string> caps;
    error = StringToStrList(caps_str, caps);
    if (error)
        return error;

    uint64_t cap_mask = 0lu;

    for (auto &cap: caps) {
        if (SupportedCaps.find(cap) == SupportedCaps.end())
            return TError(EError::InvalidValue,
                    "Unsupported capability " + cap);

        if (SupportedCaps.at(cap) > LastCapability)
            return TError(EError::InvalidValue,
                    "Unsupported kernel capability " + cap);

        cap_mask |= CAP_MASK(SupportedCaps.at(cap));

    }
    CurrentContainer->Caps = cap_mask;
    CurrentContainer->PropMask |= CAPABILITIES_SET;

    return TError::Success();
}

TError TContainerCapabilities::Get(std::string &value) {
   std::vector<std::string> caps;

    for (const auto &cap : SupportedCaps)
        if (CurrentContainer->Caps & CAP_MASK(cap.second))
            caps.push_back(cap.first);

    return StrListToString(caps, value);
}

class TContainerDefaultGw : public TContainerProperty {
public:
    TError Set(const std::string &gw);
    TError Get(std::string &value);
    TContainerDefaultGw(std::string name, uint64_t set_mask, std::string desc)
                        : TContainerProperty(name, set_mask, desc, true) {}
};

TError TContainerDefaultGw::Set(const std::string &gw) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    TNetCfg cfg;
    std::vector<std::string> gws;

    error = StringToStrList(gw, gws);
    if (error)
        return error;

    error = cfg.ParseGw(gws);
    if (error)
        return error;

    CurrentContainer->DefaultGw = gws;
    CurrentContainer->PropMask |= DEFAULT_GW_SET;

    return TError::Success();
}

TError TContainerDefaultGw::Get(std::string &value) {
    return StrListToString(CurrentContainer->DefaultGw, value);
}

class TContainerResolvConf : public TContainerProperty {
public:
    TError Set(const std::string &conf);
    TError Get(std::string &value);
    TContainerResolvConf(std::string name, uint64_t set_mask, std::string desc)
                         : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerResolvConf::Set(const std::string &conf_str) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    std::vector<std::string> conf;

    error = StringToStrList(conf_str, conf);
    if (error)
        return error;

    CurrentContainer->ResolvConf = conf;
    CurrentContainer->PropMask |= RESOLV_CONF_SET;

    return TError::Success();
}

TError TContainerResolvConf::Get(std::string &value) {
    return StrListToString(CurrentContainer->ResolvConf, value);
}

class TContainerDevices : public TContainerProperty {
public:
    TError Set(const std::string &dev);
    TError Get(std::string &value);
    TContainerDevices(std::string name, uint64_t set_mask, std::string desc)
                      : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerDevices::Set(const std::string &dev_str) {
    std::vector<std::string> dev_list;

    TError error = StringToStrList(dev_str, dev_list);
    if (error)
        return error;

    CurrentContainer->Devices = dev_list;
    CurrentContainer->PropMask |= DEVICES_SET;

    return TError::Success();
}

TError TContainerDevices::Get(std::string &value) {
    return StrListToString(CurrentContainer->Devices, value);
}

class TContainerRawRootPid : public TContainerProperty {
public:
    TError SetFromRestore(const std::string &value);
    TError Get(std::string &value);
    TContainerRawRootPid(std::string name, std::string desc)
                         : TContainerProperty(name, desc, true, true) {
        SetMask = ROOT_PID_SET;
    }
};

TError TContainerRawRootPid::SetFromRestore(const std::string &value) {
    std::vector<std::string> str_list;
    TError error = StringToStrList(value, str_list);
    if (error)
        return error;

    return StringsToIntegers(str_list, CurrentContainer->RootPid);
}

TError TContainerRawRootPid::Get(std::string &value) {
    std::stringstream str;
    bool first = true;

    for (auto v : CurrentContainer->RootPid) {
        if (first)
             first = false;
        else
             str << ";";
        str << v;
   }

   value = str.str();
   return TError::Success();
}

class TContainerRawLoopDev : public TContainerProperty {
public:
    TError SetFromRestore(const std::string &value);
    TError Get(std::string &value);
    TContainerRawLoopDev(std::string name, std::string desc)
                    : TContainerProperty(name, desc, true, true) {
        SetMask = LOOP_DEV_SET;
    }
};

TError TContainerRawLoopDev::SetFromRestore(const std::string &value) {
    return StringToInt(value, CurrentContainer->LoopDev);
}

TError TContainerRawLoopDev::Get(std::string &value) {
    value = std::to_string(CurrentContainer->LoopDev);

    return TError::Success();
}

class TContainerRawStartTime : public TContainerProperty {
public:
    TError SetFromRestore(const std::string &value);
    TError Get(std::string &value);
    TContainerRawStartTime(std::string name, std::string desc)
                           : TContainerProperty(name, desc, true, true) {
        SetMask = START_TIME_SET;
    }
};

TError TContainerRawStartTime::SetFromRestore(const std::string &value) {
    return StringToUint64(value, CurrentContainer->StartTime);
}

TError TContainerRawStartTime::Get(std::string &value) {
    value = std::to_string(CurrentContainer->StartTime);

    return TError::Success();
}

class TContainerRawDeathTime : public TContainerProperty {
public:
    TError SetFromRestore(const std::string &value);
    TError Get(std::string &value);
    TContainerRawDeathTime(std::string name, std::string desc)
                           : TContainerProperty(name, desc, true, true) {
        SetMask = DEATH_TIME_SET;
    }
};

TError TContainerRawDeathTime::SetFromRestore(const std::string &value) {
    return StringToUint64(value, CurrentContainer->DeathTime);
}

TError TContainerRawDeathTime::Get(std::string &value) {
    value = std::to_string(CurrentContainer->DeathTime);

    return TError::Success();
}

class TContainerUlimit : public TContainerProperty {
    const std::map<std::string,int> nameToIdx = {
        { "as", RLIMIT_AS },
        { "core", RLIMIT_CORE },
        { "cpu", RLIMIT_CPU },
        { "data", RLIMIT_DATA },
        { "fsize", RLIMIT_FSIZE },
        { "locks", RLIMIT_LOCKS },
        { "memlock", RLIMIT_MEMLOCK },
        { "msgqueue", RLIMIT_MSGQUEUE },
        { "nice", RLIMIT_NICE },
        { "nofile", RLIMIT_NOFILE },
        { "nproc", RLIMIT_NPROC },
        { "rss", RLIMIT_RSS },
        { "rtprio", RLIMIT_RTPRIO },
        { "rttime", RLIMIT_RTTIME },
        { "sigpending", RLIMIT_SIGPENDING },
        { "stask", RLIMIT_STACK },
    };
public:
    TError Set(const std::string &ulimit);
    TError Get(std::string &value);
    TContainerUlimit(std::string name, uint64_t set_mask, std::string desc)
                     : TContainerProperty(name, set_mask, desc) {}
    void Propagate(std::map<int, struct rlimit> &ulimits);
};

TError TContainerUlimit::Set(const std::string &ulimit_str) {
    TError error = IsAliveAndStopped();
    if (error)
        return error;

    std::vector<std::string> ulimits;
    error = StringToStrList(ulimit_str, ulimits);
    if (error)
        return error;

    /*
     * The final copy will be slow, but we don't want
     * to have inconsistent ulimits inside the container...
     */

    std::map<int,struct rlimit> new_limit;
    for (auto &limit : ulimits) {
        std::vector<std::string> nameval;

        (void)SplitString(limit, ':', nameval);
        if (nameval.size() != 2)
            return TError(EError::InvalidValue, "Invalid limits format");

        std::string name = StringTrim(nameval[0]);
        if (nameToIdx.find(name) == nameToIdx.end())
            return TError(EError::InvalidValue, "Invalid limit " + name);
        int idx = nameToIdx.at(name);

        std::vector<std::string> softhard;
        (void)SplitString(StringTrim(nameval[1]), ' ', softhard);
        if (softhard.size() != 2)
            return TError(EError::InvalidValue, "Invalid limits number for " + name);

        rlim_t soft, hard;
        if (softhard[0] == "unlim" || softhard[0] == "unlimited") {
            soft = RLIM_INFINITY;
        } else {
            TError error = StringToUint64(softhard[0], soft);
            if (error)
                return TError(EError::InvalidValue, "Invalid soft limit for " + name);
        }

        if (softhard[1] == "unlim" || softhard[1] == "unlimited") {
            hard = RLIM_INFINITY;
        } else {
            TError error = StringToUint64(softhard[1], hard);
            if (error)
                return TError(EError::InvalidValue, "Invalid hard limit for " + name);
        }

        new_limit[idx].rlim_cur = soft;
        new_limit[idx].rlim_max = hard;
    }

    CurrentContainer->Rlimit = new_limit;
    CurrentContainer->PropMask |= ULIMIT_SET;
    Propagate(new_limit);

    return TError::Success();
}

TError TContainerUlimit::Get(std::string &value) {
    std::stringstream str;
    bool first = true;

   for (auto limit_elem : nameToIdx) {
        auto value = CurrentContainer->Rlimit.find(limit_elem.second);
        if (value == CurrentContainer->Rlimit.end())
            continue;

        if (first)
             first = false;
        else
             str << ";";

        str << limit_elem.first << " " <<
            std::to_string((*value).second.rlim_cur) << " " <<
            std::to_string((*value).second.rlim_max);

   }

   value = str.str();

   return TError::Success();
}

void TContainerUlimit::Propagate(std::map<int, struct rlimit> &ulimits) {

    for (auto iter : CurrentContainer->Children) {
        if (auto child = iter.lock()) {

            auto old = CurrentContainer;
            CurrentContainer = child.get();

            if (!(CurrentContainer->PropMask & ULIMIT_SET) &&
                !(CurrentContainer->Isolate)) {

                CurrentContainer->Rlimit = ulimits;
                ContainerUlimit.Propagate(ulimits);
            }
            CurrentContainer = old;
        }
    }
}

class TContainerPortoNamespace : public TContainerProperty {
public:
    TError Set(const std::string &ns);
    TError Get(std::string &value);
    TContainerPortoNamespace(std::string name, uint64_t set_mask,
                              std::string desc)
                              : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerPortoNamespace::Set(const std::string &ns) {
    TError error = IsAlive();
    if (error)
        return error;

    CurrentContainer->NsName = ns;
    CurrentContainer->PropMask |= PORTO_NAMESPACE_SET;

    return TError::Success();
}

TError TContainerPortoNamespace::Get(std::string &value) {
    value = CurrentContainer->NsName;

    return TError::Success();
}

class TContainerStdoutLimit : public TContainerProperty {
public:
    TError Set(const std::string &limit);
    TError Get(std::string &value);
    TContainerStdoutLimit(std::string name, uint64_t set_mask,
                          std::string desc)
                          : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerStdoutLimit::Set(const std::string &limit) {
    TError error = IsAlive();
    if (error)
        return error;

    uint64_t new_size = 0lu;
    error = StringToSize(limit, new_size);
    if (error)
        return error;

    auto max = config().container().stdout_limit();
    if (new_size > max)
        return TError(EError::InvalidValue, "Maximum number of bytes: " +
                      std::to_string(max));

    CurrentContainer->StdoutLimit = new_size;
    CurrentContainer->PropMask |= STDOUT_LIMIT_SET;

    return TError::Success();
}

TError TContainerStdoutLimit::Get(std::string &value) {
    value = std::to_string(CurrentContainer->StdoutLimit);

    return TError::Success();
}

class TContainerMemoryLimit : public TContainerProperty {
public:
    TError Set(const std::string &limit);
    TError Get(std::string &value);
    TContainerMemoryLimit(std::string name, uint64_t set_mask,
                          std::string desc)
                          : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerMemoryLimit::Set(const std::string &limit) {
    TError error = IsAlive();
    if (error)
        return error;

    uint64_t new_size = 0lu;
    error = StringToSize(limit, new_size);
    if (error)
        return error;

    if (CurrentContainer->GetState() == EContainerState::Running ||
        CurrentContainer->GetState() == EContainerState::Meta ||
        CurrentContainer->GetState() == EContainerState::Paused) {

        auto memcg = CurrentContainer->GetCgroup(MemorySubsystem);
        error = MemorySubsystem.SetLimit(memcg, new_size);

        if (error) {
            if (error.GetErrno() == EBUSY)
                return TError(EError::InvalidValue,
                              std::string(P_MEM_LIMIT) + " is too low");

            L_ERR() << "Can't set " << P_MEM_LIMIT << ": " << error << std::endl;

            return error;
        }
    }

    CurrentContainer->MemLimit = new_size;
    CurrentContainer->PropMask |= MEM_LIMIT_SET;

    return TError::Success();
}

TError TContainerMemoryLimit::Get(std::string &value) {
    value = std::to_string(CurrentContainer->MemLimit);

    return TError::Success();
}

class TContainerAnonLimit : public TContainerProperty {
public:
    TError Set(const std::string &limit);
    TError Get(std::string &value);
    TContainerAnonLimit(std::string name, uint64_t set_mask,
                        std::string desc)
                        : TContainerProperty(name, set_mask, desc) {}
    TError Init(void) {
        IsSupported = MemorySubsystem.SupportAnonLimit();

        return TError::Success();
    }

};

TError TContainerAnonLimit::Set(const std::string &limit) {
    TError error = IsAlive();
    if (error)
        return error;

    uint64_t new_size = 0lu;
    error = StringToSize(limit, new_size);
    if (error)
        return error;

    if (CurrentContainer->GetState() == EContainerState::Running ||
        CurrentContainer->GetState() == EContainerState::Meta ||
        CurrentContainer->GetState() == EContainerState::Paused) {

        auto memcg = CurrentContainer->GetCgroup(MemorySubsystem);
        error = MemorySubsystem.SetAnonLimit(memcg, new_size);

        if (error) {
            L_ERR() << "Can't set " << P_ANON_LIMIT << ": " << error << std::endl;

            return error;
        }
    }

    CurrentContainer->AnonMemLimit = new_size;
    CurrentContainer->PropMask |= ANON_LIMIT_SET;

    return TError::Success();
}

TError TContainerAnonLimit::Get(std::string &value) {
    value = std::to_string(CurrentContainer->AnonMemLimit);

    return TError::Success();
}

class TContainerDirtyLimit : public TContainerProperty {
public:
    TError Set(const std::string &limit);
    TError Get(std::string &value);
    TContainerDirtyLimit(std::string name, uint64_t set_mask,
                        std::string desc)
                        : TContainerProperty(name, set_mask, desc) {}
    TError Init(void) {
        IsSupported = MemorySubsystem.SupportDirtyLimit();

        return TError::Success();
    }

};

TError TContainerDirtyLimit::Set(const std::string &limit) {
    TError error = IsAlive();
    if (error)
        return error;

    uint64_t new_size = 0lu;
    error = StringToSize(limit, new_size);
    if (error)
        return error;

    if (CurrentContainer->GetState() == EContainerState::Running ||
        CurrentContainer->GetState() == EContainerState::Meta ||
        CurrentContainer->GetState() == EContainerState::Paused) {

        auto memcg = CurrentContainer->GetCgroup(MemorySubsystem);
        error = MemorySubsystem.SetDirtyLimit(memcg, new_size);

        if (error) {
            L_ERR() << "Can't set " << P_ANON_LIMIT << ": " << error << std::endl;

            return error;
        }
    }

    CurrentContainer->DirtyMemLimit = new_size;
    CurrentContainer->PropMask |= ANON_LIMIT_SET;

    return TError::Success();
}

TError TContainerDirtyLimit::Get(std::string &value) {
    value = std::to_string(CurrentContainer->DirtyMemLimit);

    return TError::Success();
}

class TContainerRechargeOnPgfault : public TContainerProperty {
public:
    TError Set(const std::string &recharge);
    TError Get(std::string &value);
    TContainerRechargeOnPgfault(std::string name, uint64_t set_mask,
                        std::string desc)
                        : TContainerProperty(name, set_mask, desc) {}
    TError Init(void) {
        IsSupported = MemorySubsystem.SupportRechargeOnPgfault();

        return TError::Success();
    }

};

TError TContainerRechargeOnPgfault::Set(const std::string &recharge) {
    TError error = IsAlive();
    if (error)
        return error;

    bool new_val;
    if (recharge == "true")
        new_val = true;
    else if (recharge == "false")
        new_val = false;
    else
        return TError(EError::InvalidValue, "Invalid bool value");

    if (CurrentContainer->GetState() == EContainerState::Running ||
        CurrentContainer->GetState() == EContainerState::Meta ||
        CurrentContainer->GetState() == EContainerState::Paused) {

        auto memcg = CurrentContainer->GetCgroup(MemorySubsystem);
        error = MemorySubsystem.RechargeOnPgfault(memcg, new_val);

        if (error) {
            L_ERR() << "Can't set " << P_ANON_LIMIT << ": " << error << std::endl;
            return error;
        }
    }

    CurrentContainer->RechargeOnPgfault = new_val;
    CurrentContainer->PropMask |= RECHARGE_ON_PGFAULT_SET;

    return TError::Success();
}

TError TContainerRechargeOnPgfault::Get(std::string &value) {
    value = CurrentContainer->RechargeOnPgfault ? "true" : "false";

    return TError::Success();
}

class TContainerCpuPolicy : public TContainerProperty {
public:
    TError Set(const std::string &policy);
    TError Get(std::string &value);
    TContainerCpuPolicy(std::string name, uint64_t set_mask,
                        std::string desc)
                        : TContainerProperty(name, set_mask, desc) {}
    TError Propagate(const std::string &policy);
};

TError TContainerCpuPolicy::Set(const std::string &policy) {
    TError error = IsAlive();
    if (error)
        return error;

    if (policy != "normal" && policy != "rt" && policy != "idle")
        return TError(EError::InvalidValue, "invalid policy");


    if (CurrentContainer->GetState() == EContainerState::Running ||
        CurrentContainer->GetState() == EContainerState::Meta ||
        CurrentContainer->GetState() == EContainerState::Paused) {

        auto cpucg = CurrentContainer->GetCgroup(CpuSubsystem);
        error = CpuSubsystem.SetCpuPolicy(cpucg, policy,
                                          CurrentContainer->CpuGuarantee,
                                          CurrentContainer->CpuLimit);

        if (error) {
            L_ERR() << "Cannot set cpu policy: " << error << std::endl;
            return error;
        }

    }

    error = Propagate(policy);
    if (error)
        return error;

    CurrentContainer->CpuPolicy = policy;
    CurrentContainer->PropMask |= CPU_POLICY_SET;

    return TError::Success();
}

TError TContainerCpuPolicy::Get(std::string &value) {
    value = CurrentContainer->CpuPolicy;

    return TError::Success();
}

TError TContainerCpuPolicy::Propagate(const std::string &policy) {
    TError error = TError::Success();

    for (auto iter : CurrentContainer->Children) {
        if (auto child = iter.lock()) {

            auto old = CurrentContainer;
            CurrentContainer = child.get();

            if (!(CurrentContainer->PropMask & CPU_POLICY_SET) &&
                !(CurrentContainer->Isolate)) {

                if (CurrentContainer->GetState() == EContainerState::Running ||
                    CurrentContainer->GetState() == EContainerState::Meta ||
                    CurrentContainer->GetState() == EContainerState::Paused) {

                    auto cpucg = CurrentContainer->GetCgroup(CpuSubsystem);
                    error = CpuSubsystem.SetCpuPolicy(cpucg, policy,
                                                      CurrentContainer->CpuGuarantee,
                                                      CurrentContainer->CpuLimit);
                }

                if (error) {
                    L_ERR() << "Cannot set cpu policy: " << error << std::endl;
                } else {
                    /* FIXME: Inconsistent state of child tree cpu policy! */
                    CurrentContainer->CpuPolicy = policy;
                    error = Propagate(policy);
                }
            }

            CurrentContainer = old;
            if (error)
                break;
        }
    }

    return error;
}

class TContainerCpuLimit : public TContainerProperty {
public:
    TError Set(const std::string &limit);
    TError Get(std::string &value);
    TContainerCpuLimit(std::string name, uint64_t set_mask,
                       std::string desc)
                       : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerCpuLimit::Set(const std::string &limit) {
    TError error = IsAlive();
    if (error)
        return error;

    double new_limit;
    error = StringToCpuValue(limit, new_limit);
    if (error)
        return error;

    if (CurrentContainer->GetState() == EContainerState::Running ||
        CurrentContainer->GetState() == EContainerState::Meta ||
        CurrentContainer->GetState() == EContainerState::Paused) {

        auto cpucg = CurrentContainer->GetCgroup(CpuSubsystem);
        error = CpuSubsystem.SetCpuPolicy(cpucg, CurrentContainer->CpuPolicy,
                                          CurrentContainer->CpuGuarantee,
                                          new_limit);

        if (error) {
            L_ERR() << "Cannot set cpu policy: " << error << std::endl;
            return error;
        }

    }

    CurrentContainer->CpuLimit = new_limit;
    CurrentContainer->PropMask |= CPU_LIMIT_SET;

    return TError::Success();
}

TError TContainerCpuLimit::Get(std::string &value) {
    value = StringFormat("%lgc", CurrentContainer->CpuLimit);

    return TError::Success();
}

class TContainerCpuGuarantee : public TContainerProperty {
public:
    TError Set(const std::string &guarantee);
    TError Get(std::string &value);
    TContainerCpuGuarantee(std::string name, uint64_t set_mask,
                           std::string desc)
                           : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerCpuGuarantee::Set(const std::string &guarantee) {
    TError error = IsAlive();
    if (error)
        return error;

    double new_guarantee;
    error = StringToCpuValue(guarantee, new_guarantee);
    if (error)
        return error;

    if (CurrentContainer->GetState() == EContainerState::Running ||
        CurrentContainer->GetState() == EContainerState::Meta ||
        CurrentContainer->GetState() == EContainerState::Paused) {

        auto cpucg = CurrentContainer->GetCgroup(CpuSubsystem);
        error = CpuSubsystem.SetCpuPolicy(cpucg, CurrentContainer->CpuPolicy,
                                          new_guarantee,
                                          CurrentContainer->CpuLimit);

        if (error) {
            L_ERR() << "Cannot set cpu policy: " << error << std::endl;
            return error;
        }

    }

    CurrentContainer->CpuGuarantee = new_guarantee;
    CurrentContainer->PropMask |= CPU_GUARANTEE_SET;

    return TError::Success();
}

TError TContainerCpuGuarantee::Get(std::string &value) {
    value = StringFormat("%lgc", CurrentContainer->CpuGuarantee);

    return TError::Success();
}

class TContainerIoPolicy : public TContainerProperty {
public:
    TError Set(const std::string &policy);
    TError Get(std::string &value);
    TContainerIoPolicy(std::string name, uint64_t set_mask,
                       std::string desc)
                       : TContainerProperty(name, set_mask, desc) {}
    TError Init(void) {
        IsSupported = BlkioSubsystem.SupportPolicy();

        return TError::Success();
    }
    TError Propagate(const std::string &policy);
};

TError TContainerIoPolicy::Set(const std::string &policy) {
    TError error = IsAlive();
    if (error)
        return error;

    if (policy != "normal" && policy != "batch")
        return TError(EError::InvalidValue, "invalid policy");


    if (CurrentContainer->GetState() == EContainerState::Running ||
        CurrentContainer->GetState() == EContainerState::Meta ||
        CurrentContainer->GetState() == EContainerState::Paused) {

        auto blkcg = CurrentContainer->GetCgroup(BlkioSubsystem);
        error = BlkioSubsystem.SetPolicy(blkcg, policy == "batch");

        if (error) {
            L_ERR() << "Can't set " << P_IO_POLICY << ": " << error << std::endl;
            return error;
        }

    }

    error = Propagate(policy);
    if (error)
        return error;

    CurrentContainer->IoPolicy = policy;
    CurrentContainer->PropMask |= IO_POLICY_SET;

    return TError::Success();
}

TError TContainerIoPolicy::Get(std::string &value) {
    value = CurrentContainer->IoPolicy;

    return TError::Success();
}

TError TContainerIoPolicy::Propagate(const std::string &policy) {
    TError error = TError::Success();

    for (auto iter : CurrentContainer->Children) {
        if (auto child = iter.lock()) {

            auto old = CurrentContainer;
            CurrentContainer = child.get();

            if (!(CurrentContainer->PropMask & IO_POLICY_SET) &&
                !(CurrentContainer->Isolate)) {

                if (CurrentContainer->GetState() == EContainerState::Running ||
                    CurrentContainer->GetState() == EContainerState::Meta ||
                    CurrentContainer->GetState() == EContainerState::Paused) {

                    auto blkcg = CurrentContainer->GetCgroup(BlkioSubsystem);
                    error = BlkioSubsystem.SetPolicy(blkcg, policy == "batch");
                }

                if (error) {
                    L_ERR() << "Can't set " << P_IO_POLICY << ": " << error
                            << std::endl;
                } else {
                    /* FIXME: Inconsistent state of child tree IO policy! */
                    CurrentContainer->IoPolicy = policy;
                    error = Propagate(policy);
                }
            }

            CurrentContainer = old;
            if (error)
                break;
        }
    }

    return error;
}

class TContainerIoLimit : public TContainerProperty {
public:
    TError Set(const std::string &limit);
    TError Get(std::string &value);
    TContainerIoLimit(std::string name, uint64_t set_mask,
                      std::string desc)
                      : TContainerProperty(name, set_mask, desc) {}
    TError Init(void) {
        IsSupported = MemorySubsystem.SupportIoLimit();

        return TError::Success();
    }
};

TError TContainerIoLimit::Set(const std::string &limit) {
    TError error = IsAlive();
    if (error)
        return error;

    uint64_t new_limit = 0lu;
    error = StringToSize(limit, new_limit);
    if (error)
        return error;

    if (CurrentContainer->GetState() == EContainerState::Running ||
        CurrentContainer->GetState() == EContainerState::Meta ||
        CurrentContainer->GetState() == EContainerState::Paused) {

        auto memcg = CurrentContainer->GetCgroup(MemorySubsystem);
        error = MemorySubsystem.SetIoLimit(memcg, new_limit);
        if (error) {
            L_ERR() << "Can't set " << P_IO_LIMIT << ": " << error << std::endl;
            return error;
        }
    }

    CurrentContainer->IoLimit = new_limit;
    CurrentContainer->PropMask |= IO_LIMIT_SET;

    return TError::Success();
}

TError TContainerIoLimit::Get(std::string &value) {
    value = std::to_string(CurrentContainer->IoLimit);

    return TError::Success();
}

class TContainerIopsLimit : public TContainerProperty {
public:
    TError Set(const std::string &limit);
    TError Get(std::string &value);
    TContainerIopsLimit(std::string name, uint64_t set_mask,
                        std::string desc)
                        : TContainerProperty(name, set_mask, desc) {}
    TError Init(void) {
        IsSupported = MemorySubsystem.SupportIoLimit();

        return TError::Success();
    }
};

TError TContainerIopsLimit::Set(const std::string &limit) {
    TError error = IsAlive();
    if (error)
        return error;

    uint64_t new_limit = 0lu;
    error = StringToSize(limit, new_limit);
    if (error)
        return error;

    if (CurrentContainer->GetState() == EContainerState::Running ||
        CurrentContainer->GetState() == EContainerState::Meta ||
        CurrentContainer->GetState() == EContainerState::Paused) {

        auto memcg = CurrentContainer->GetCgroup(MemorySubsystem);
        error = MemorySubsystem.SetIopsLimit(memcg, new_limit);
        if (error) {
            L_ERR() << "Can't set " << P_IO_OPS_LIMIT << ": " << error << std::endl;
            return error;
        }
    }

    CurrentContainer->IopsLimit = new_limit;
    CurrentContainer->PropMask |= IO_OPS_LIMIT_SET;

    return TError::Success();
}

TError TContainerIopsLimit::Get(std::string &value) {
    value = std::to_string(CurrentContainer->IopsLimit);

    return TError::Success();
}

class TContainerNetGuarantee : public TContainerProperty {
public:
    TError Set(const std::string &guarantee);
    TError Get(std::string &value);
    TError SetIndexed(const std::string &index, const std::string &guarantee);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerNetGuarantee(std::string name, uint64_t set_mask,
                           std::string desc)
                           : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerNetGuarantee::Set(const std::string &guarantee) {
    TError error = IsAlive();
    if (error)
        return error;

    TUintMap new_guarantee;
    error = StringToUintMap(guarantee, new_guarantee);
    if (error)
        return error;

    for (auto &kv : new_guarantee) {
        if (kv.second > NET_MAX_GUARANTEE)
            return TError(EError::InvalidValue, "Net guarantee too large");
    }

    TUintMap old_guarantee = CurrentContainer->NetGuarantee;
    CurrentContainer->NetGuarantee = new_guarantee;

    error = CurrentContainer->UpdateTrafficClasses();
    if (!error) {
        CurrentContainer->PropMask |= NET_GUARANTEE_SET;
    } else {
        L_ERR() << "Cannot update tc : " << error << std::endl;
        CurrentContainer->NetGuarantee = old_guarantee;
    }

    return error;
}

TError TContainerNetGuarantee::Get(std::string &value) {
    return UintMapToString(CurrentContainer->NetGuarantee, value);
}

TError TContainerNetGuarantee::SetIndexed(const std::string &index,
                                          const std::string &guarantee) {
    TError error = IsAlive();
    if (error)
        return error;

    uint64_t val;
    error = StringToSize(guarantee, val);
    if (error)
        return TError(EError::InvalidValue, "Invalid value " + guarantee);

    if (val > NET_MAX_GUARANTEE)
        return TError(EError::InvalidValue, "Net guarantee too large");

    uint64_t old_guarantee = CurrentContainer->NetGuarantee[index];
    CurrentContainer->NetGuarantee[index] = val;

    error = CurrentContainer->UpdateTrafficClasses();
    if (!error) {
        CurrentContainer->PropMask |= NET_GUARANTEE_SET;
    } else {
        L_ERR() << "Cannot update tc : " << error << std::endl;
        CurrentContainer->NetGuarantee[index] = old_guarantee;
    }

    return error;
}

TError TContainerNetGuarantee::GetIndexed(const std::string &index,
                                          std::string &value) {

    if (CurrentContainer->NetGuarantee.find(index) ==
        CurrentContainer->NetGuarantee.end())

        return TError(EError::InvalidValue, "invalid index " + index);

    value = std::to_string(CurrentContainer->NetGuarantee[index]);

    return TError::Success();
}

class TContainerNetLimit : public TContainerProperty {
public:
    TError Set(const std::string &limit);
    TError Get(std::string &value);
    TError SetIndexed(const std::string &index, const std::string &limit);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerNetLimit(std::string name, uint64_t set_mask,
                           std::string desc)
                           : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerNetLimit::Set(const std::string &limit) {
    TError error = IsAlive();
    if (error)
        return error;

    TUintMap new_limit;
    error = StringToUintMap(limit, new_limit);
    if (error)
        return error;

    for (auto &kv : new_limit) {
        if (kv.second > NET_MAX_LIMIT)
            return TError(EError::InvalidValue, "Net limit too large");
    }

    TUintMap old_limit = CurrentContainer->NetLimit;
    CurrentContainer->NetLimit = new_limit;

    error = CurrentContainer->UpdateTrafficClasses();
    if (!error) {
        CurrentContainer->PropMask |= NET_LIMIT_SET;
    } else {
        L_ERR() << "Cannot update tc : " << error << std::endl;
        CurrentContainer->NetLimit = old_limit;
    }

    return error;
}

TError TContainerNetLimit::Get(std::string &value) {
    return UintMapToString(CurrentContainer->NetLimit, value);
}

TError TContainerNetLimit::SetIndexed(const std::string &index,
                                      const std::string &limit) {
    TError error = IsAlive();
    if (error)
        return error;

    uint64_t val;
    error = StringToSize(limit, val);
    if (error)
        return TError(EError::InvalidValue, "Invalid value " + limit);

    if (val > NET_MAX_LIMIT)
        return TError(EError::InvalidValue, "Net limit too large");

    uint64_t old_limit = CurrentContainer->NetLimit[index];
    CurrentContainer->NetLimit[index] = val;

    error = CurrentContainer->UpdateTrafficClasses();
    if (!error) {
        CurrentContainer->PropMask |= NET_LIMIT_SET;
    } else {
        L_ERR() << "Cannot update tc : " << error << std::endl;
        CurrentContainer->NetLimit[index] = old_limit;
    }

    return error;
}

TError TContainerNetLimit::GetIndexed(const std::string &index,
                                      std::string &value) {

    if (CurrentContainer->NetLimit.find(index) ==
        CurrentContainer->NetLimit.end())

        return TError(EError::InvalidValue, "invalid index " + index);

    value = std::to_string(CurrentContainer->NetLimit[index]);

    return TError::Success();
}

class TContainerNetPriority : public TContainerProperty {
public:
    TError Set(const std::string &prio);
    TError Get(std::string &value);
    TError SetIndexed(const std::string &index, const std::string &prio);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerNetPriority(std::string name, uint64_t set_mask,
                           std::string desc)
                           : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerNetPriority::Set(const std::string &prio) {
    TError error = IsAlive();
    if (error)
        return error;

    TUintMap new_prio;
    error = StringToUintMap(prio, new_prio);
    if (error)
        return error;

    for (auto &kv : new_prio) {
        if (kv.second > 7)
            return TError(EError::InvalidValue, "invalid value");
    }

    TUintMap old_prio = CurrentContainer->NetPriority;
    CurrentContainer->NetPriority = new_prio;

    error = CurrentContainer->UpdateTrafficClasses();
    if (!error) {
        CurrentContainer->PropMask |= NET_PRIO_SET;
    } else {
        L_ERR() << "Cannot update tc : " << error << std::endl;
        CurrentContainer->NetPriority = old_prio;
    }

    return error;
}

TError TContainerNetPriority::Get(std::string &value) {
    return UintMapToString(CurrentContainer->NetPriority, value);
}

TError TContainerNetPriority::SetIndexed(const std::string &index,
                                      const std::string &prio) {
    TError error = IsAlive();
    if (error)
        return error;

    uint64_t val;
    error = StringToSize(prio, val);
    if (error)
        return TError(EError::InvalidValue, "Invalid value " + prio);

    if (val > 7)
        return TError(EError::InvalidValue, "invalid value");

    uint64_t old_prio = CurrentContainer->NetPriority[index];
    CurrentContainer->NetPriority[index] = val;

    error = CurrentContainer->UpdateTrafficClasses();
    if (!error) {
        CurrentContainer->PropMask |= NET_PRIO_SET;
    } else {
        L_ERR() << "Cannot update tc : " << error << std::endl;
        CurrentContainer->NetPriority[index] = old_prio;
    }

    return error;
}

TError TContainerNetPriority::GetIndexed(const std::string &index,
                                      std::string &value) {

    if (CurrentContainer->NetPriority.find(index) ==
        CurrentContainer->NetPriority.end())

        return TError(EError::InvalidValue, "invalid index " + index);

    value = std::to_string(CurrentContainer->NetPriority[index]);

    return TError::Success();
}

class TContainerRespawn : public TContainerProperty {
public:
    TError Set(const std::string &respawn);
    TError Get(std::string &value);
    TContainerRespawn(std::string name, uint64_t set_mask,
                      std::string desc)
                      : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerRespawn::Set(const std::string &respawn) {
    TError error = IsAlive();
    if (error)
        return error;

    if (respawn == "true")
        CurrentContainer->ToRespawn = true;
    else if (respawn == "false")
        CurrentContainer->ToRespawn = false;
    else
        return TError(EError::InvalidValue, "Invalid bool value");

    CurrentContainer->PropMask |= RESPAWN_SET;

    return TError::Success();
}

TError TContainerRespawn::Get(std::string &value) {
    value = CurrentContainer->ToRespawn ? "true" : "false";

    return TError::Success();
}

class TContainerMaxRespawns : public TContainerProperty {
public:
    TError Set(const std::string &max);
    TError Get(std::string &value);
    TContainerMaxRespawns(std::string name, uint64_t set_mask,
                          std::string desc)
                          : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerMaxRespawns::Set(const std::string &max) {
    TError error = IsAlive();
    if (error)
        return error;

    int new_value;
    if (StringToInt(max, new_value))
        return TError(EError::InvalidValue, "Invalid integer value " + max);

    CurrentContainer->MaxRespawns = new_value;
    CurrentContainer->PropMask |= MAX_RESPAWNS_SET;

    return TError::Success();
}

TError TContainerMaxRespawns::Get(std::string &value) {
    value = std::to_string(CurrentContainer->MaxRespawns);

    return TError::Success();
}

class TContainerPrivate : public TContainerProperty {
public:
    TError Set(const std::string &max);
    TError Get(std::string &value);
    TContainerPrivate(std::string name, uint64_t set_mask,
                      std::string desc)
                      : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerPrivate::Set(const std::string &value) {
    TError error = IsAlive();
    if (error)
        return error;

    uint32_t max = config().container().private_max();
    if (value.length() > max)
        return TError(EError::InvalidValue, "Value is too long");

    CurrentContainer->Private = value;

    return TError::Success();
}

TError TContainerPrivate::Get(std::string &value) {
    value = CurrentContainer->Private;

    return TError::Success();
}

class TContainerAgingTime : public TContainerProperty {
public:
    TError Set(const std::string &time);
    TError Get(std::string &value);
    TContainerAgingTime(std::string name, uint64_t set_mask, std::string desc)
                        : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerAgingTime::Set(const std::string &time) {
    TError error = IsAlive();
    if (error)
        return error;

    uint64_t new_time;
    error = StringToUint64(time, new_time);
    if (error)
        return error;

    CurrentContainer->AgingTime = new_time;
    CurrentContainer->PropMask |= AGING_TIME_SET;

    return TError::Success();
}

TError TContainerAgingTime::Get(std::string &value) {
    value = std::to_string(CurrentContainer->AgingTime);

    return TError::Success();
}

class TContainerEnablePorto : public TContainerProperty {
public:
    TError Set(const std::string &enabled);
    TError Get(std::string &value);
    TContainerEnablePorto(std::string name, uint64_t set_mask, std::string desc)
                          : TContainerProperty(name, set_mask, desc) {}
    void Propagate(bool value);
};

TError TContainerEnablePorto::Set(const std::string &enabled) {
    TError error = IsAlive();
    if (error)
        return error;

    if (enabled == "true") {
        auto p = CurrentContainer->GetParent();
        if (p && !p->PortoEnabled)
            return TError(EError::InvalidValue, "Porto disabled in parent container");

        CurrentContainer->PortoEnabled = true;
        Propagate(true);

    } else if (enabled == "false") {
        CurrentContainer->PortoEnabled = false;
        Propagate(false);

    } else {
        return TError(EError::InvalidValue, "Invalid bool value");
    }

    CurrentContainer->PropMask |= ENABLE_PORTO_SET;

    return TError::Success();
}

TError TContainerEnablePorto::Get(std::string &value) {
    value = CurrentContainer->PortoEnabled ? "true" : "false";

    return TError::Success();
}

void TContainerEnablePorto::Propagate(bool value) {
    for (auto iter : CurrentContainer->Children) {
        if (auto child = iter.lock()) {
            auto old = CurrentContainer;
            CurrentContainer = child.get();

            if (!(CurrentContainer->PropMask & ENABLE_PORTO_SET))
                CurrentContainer->PortoEnabled = value;

            Propagate(value);
            CurrentContainer = old;
        }
    }
}

class TContainerWeak : public TContainerProperty {
public:
    TError Set(const std::string &weak);
    TError Get(std::string &value);
    TContainerWeak(std::string name, uint64_t set_mask, std::string desc)
                          : TContainerProperty(name, set_mask, desc) {}
};

TError TContainerWeak::Set(const std::string &weak) {
    TError error = IsAlive();
    if (error)
        return error;

    if (weak == "true")
        CurrentContainer->IsWeak = true;
    else if (weak == "false")
        CurrentContainer->IsWeak = false;
    else
        return TError(EError::InvalidValue, "Invalid bool value");

    CurrentContainer->PropMask |= WEAK_SET;

    return TError::Success();
}

TError TContainerWeak::Get(std::string &value) {
    value = CurrentContainer->IsWeak ? "true" : "false";

    return TError::Success();
}

/* Read-only properties derived from data filelds follow below... */

class TContainerAbsoluteName : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerAbsoluteName(std::string name, std::string desc)
                           : TContainerProperty(name, desc) {}
};

TError TContainerAbsoluteName::Get(std::string &value) {
    if (CurrentContainer->IsRoot() || CurrentContainer->IsPortoRoot())
        value = CurrentContainer->GetName();
    else
        value = std::string(PORTO_ROOT_CONTAINER) + "/" +
                CurrentContainer->GetName();

    return TError::Success();
}

class TContainerAbsoluteNamespace : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerAbsoluteNamespace(std::string name, std::string desc)
                                : TContainerProperty(name, desc) {}
};

TError TContainerAbsoluteNamespace::Get(std::string &value) {
    value = std::string(PORTO_ROOT_CONTAINER) + "/" +
            CurrentContainer->GetPortoNamespace();

    return TError::Success();
}

class TContainerState : public TContainerProperty {
public:
    TError SetFromRestore(const std::string &value);
    TError Get(std::string &value);
    TContainerState(std::string name, std::string desc)
                    : TContainerProperty(name, desc, false, true) {
        SetMask = STATE_SET;
    }
};

TError TContainerState::SetFromRestore(const std::string &value) {
    /*
     * We are just restoring value indication there.
     * The container must manually call SetState()
     * TContainer::Restore() handler.
     */

    if (value == "stopped")
        CurrentContainer->State = EContainerState::Stopped;
    else if (value == "dead")
        CurrentContainer->State = EContainerState::Dead;
    else if (value == "running")
        CurrentContainer->State = EContainerState::Running;
    else if (value == "paused")
        CurrentContainer->State = EContainerState::Paused;
    else if (value == "meta")
        CurrentContainer->State = EContainerState::Meta;
    else if (value == "unknown")
        CurrentContainer->State  = EContainerState::Unknown;
    else
        return TError(EError::Unknown, "Invalid container saved state");

    return TError::Success();
}

TError TContainerState::Get(std::string &value) {
    value = CurrentContainer->ContainerStateName(CurrentContainer->GetState());

    return TError::Success();
}

class TContainerOomKilled : public TContainerProperty {
public:
    TError SetFromRestore(const std::string &value);
    TError GetToSave(std::string &value);
    TError Get(std::string &value);
    TContainerOomKilled(std::string name, std::string desc)
                    : TContainerProperty(name, desc, false, true) {
        SetMask = OOM_KILLED_SET;
    }
};

TError TContainerOomKilled::SetFromRestore(const std::string &value) {
    if (value == "true")
        CurrentContainer->OomKilled = true;
    else if (value == "false")
        CurrentContainer->OomKilled = false;
    else
        return TError(EError::InvalidValue, "Invalid bool value");

    return TError::Success();
}

TError TContainerOomKilled::GetToSave(std::string &value) {
    value = CurrentContainer->OomKilled ? "true" : "false";

    return TError::Success();
}

TError TContainerOomKilled::Get(std::string &value) {
    TError error = IsDead();
    if (error)
        return error;

    return GetToSave(value);
}

class TContainerParent : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerParent(std::string name, std::string desc)
                     : TContainerProperty(name, desc, true, false) {}
};

TError TContainerParent::Get(std::string &value) {
    auto p = CurrentContainer->GetParent();
    value = p ? p->GetName() : "";

    return TError::Success();
}

class TContainerRespawnCount : public TContainerProperty {
public:
    TError SetFromRestore(const std::string &value);
    TError Get(std::string &value);
    TContainerRespawnCount(std::string name, std::string desc)
                           : TContainerProperty(name, desc, false, true) {
        SetMask = RESPAWN_COUNT_SET;
    }
};

TError TContainerRespawnCount::SetFromRestore(const std::string &value) {
    return StringToUint64(value, CurrentContainer->RespawnCount);
}

TError TContainerRespawnCount::Get(std::string &value) {
    value = std::to_string(CurrentContainer->RespawnCount);

    return TError::Success();
}

class TContainerRootPid : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerRootPid(std::string name, std::string desc)
                      : TContainerProperty(name, desc, true) {}
};

TError TContainerRootPid::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;
   
    if (CurrentContainer->Task && CurrentClient)
        value = std::to_string(CurrentContainer->Task->
                               GetPidFor(CurrentClient->GetPid()));
    else
        value = "";

    return TError::Success();
}

class TContainerExitStatus : public TContainerProperty {
public:
    TError SetFromRestore(const std::string &value);
    TError GetToSave(std::string &value);
    TError Get(std::string &value);
    TContainerExitStatus(std::string name, std::string desc)
                         : TContainerProperty(name, desc, false, true) {
        SetMask = EXIT_STATUS_SET;
    }
};

TError TContainerExitStatus::SetFromRestore(const std::string &value) {
    return StringToInt(value, CurrentContainer->ExitStatus);
}

TError TContainerExitStatus::GetToSave(std::string &value) {
    value = std::to_string(CurrentContainer->ExitStatus);

    return TError::Success();
}

TError TContainerExitStatus::Get(std::string &value) {
    TError error = IsDead();
    if (error)
        return error;

    return GetToSave(value);
}

class TContainerStartErrno : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerStartErrno(std::string name, std::string desc)
                         : TContainerProperty(name, desc) {}
};

TError TContainerStartErrno::Get(std::string &value) {
    value = std::to_string(CurrentContainer->TaskStartErrno);

    return TError::Success();
}

class TContainerStdout : public TContainerProperty {
public:
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerStdout(std::string name, std::string desc)
                     : TContainerProperty(name, desc) {}
};

TError TContainerStdout::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    return CurrentContainer->GetStdout().Read(value,
                                              CurrentContainer->StdoutLimit,
                                              CurrentContainer->StdoutOffset);
}

TError TContainerStdout::GetIndexed(const std::string &index,
                                    std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    return CurrentContainer->GetStdout().Read(value,
                                              CurrentContainer->StdoutLimit,
                                              CurrentContainer->StdoutOffset,
                                              index);
}

class TContainerStdoutOffset : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerStdoutOffset(std::string name, std::string desc)
                           : TContainerProperty(name, desc) {}
};

TError TContainerStdoutOffset::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    value = std::to_string(CurrentContainer->StdoutOffset);

    return TError::Success();
}

class TContainerStderr : public TContainerProperty {
public:
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerStderr(std::string name, std::string desc)
                     : TContainerProperty(name, desc) {}
};

TError TContainerStderr::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    return CurrentContainer->GetStderr().Read(value,
                                              CurrentContainer->StdoutLimit,
                                              CurrentContainer->StderrOffset);
}

TError TContainerStderr::GetIndexed(const std::string &index,
                                    std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    return CurrentContainer->GetStderr().Read(value,
                                              CurrentContainer->StdoutLimit,
                                              CurrentContainer->StderrOffset,
                                              index);
}

class TContainerStderrOffset : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerStderrOffset(std::string name, std::string desc)
                           : TContainerProperty(name, desc) {}
};

TError TContainerStderrOffset::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    value = std::to_string(CurrentContainer->StderrOffset);

    return TError::Success();
}

class TContainerMemUsage : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerMemUsage(std::string name, std::string desc)
                       : TContainerProperty(name, desc) {}
};

TError TContainerMemUsage::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    auto cg = CurrentContainer->GetCgroup(MemorySubsystem);

    uint64_t val;
    error = MemorySubsystem.Usage(cg, val);
    if (error) {
        L_ERR() << "Can't get memory usage: " << error << std::endl;
        return error;
    }

    value = std::to_string(val);

    return TError::Success();
}

class TContainerAnonUsage : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerAnonUsage(std::string name, std::string desc)
                       : TContainerProperty(name, desc) {}
};

TError TContainerAnonUsage::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    auto cg = CurrentContainer->GetCgroup(MemorySubsystem);
    uint64_t val;

    if (MemorySubsystem.GetAnonUsage(cg, val))
        value = "0";
    else
        value = std::to_string(val);

    return TError::Success();
}

class TContainerMinorFaults : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerMinorFaults(std::string name, std::string desc)
                          : TContainerProperty(name, desc) {}
};

TError TContainerMinorFaults::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    auto cg = CurrentContainer->GetCgroup(MemorySubsystem);
    TUintMap stat;

    if (MemorySubsystem.Statistics(cg, stat))
        value = "-1";
    else
        value = std::to_string(stat["total_pgfault"] - stat["total_pgmajfault"]);

    return TError::Success();
}

class TContainerMajorFaults : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerMajorFaults(std::string name, std::string desc)
                          : TContainerProperty(name, desc) {}
};

TError TContainerMajorFaults::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    auto cg = CurrentContainer->GetCgroup(MemorySubsystem);
    TUintMap stat;

    if (MemorySubsystem.Statistics(cg, stat))
        value = "-1";
    else
        value = std::to_string(stat["total_pgmajfault"]);

    return TError::Success();
}

class TContainerMaxRss : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerMaxRss(std::string name, std::string desc)
                     : TContainerProperty(name, desc) {}
    TError Init(void) {
        TCgroup rootCg = MemorySubsystem.RootCgroup();
        TUintMap stat;

        TError error = MemorySubsystem.Statistics(rootCg, stat);
        IsSupported = !error && (stat.find("total_max_rss") != stat.end());

        return TError::Success();
    }
};

TError TContainerMaxRss::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    auto cg = CurrentContainer->GetCgroup(MemorySubsystem);
    TUintMap stat;
    if (MemorySubsystem.Statistics(cg, stat))
        value = "-1";
    else
        value = std::to_string(stat["total_max_rss"]);

    return TError::Success();
}

class TContainerCpuUsage : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerCpuUsage(std::string name, std::string desc)
                       : TContainerProperty(name, desc) {}
};

TError TContainerCpuUsage::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    auto cg = CurrentContainer->GetCgroup(CpuacctSubsystem);

    uint64_t val;
    error = CpuacctSubsystem.Usage(cg, val);

    if (error) {
        L_ERR() << "Can't get CPU usage: " << error << std::endl;
        value = "-1";
    } else {
        value = std::to_string(val);
    }

    return TError::Success();
}

class TContainerCpuSystem : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerCpuSystem(std::string name, std::string desc)
                        : TContainerProperty(name, desc) {}
};

TError TContainerCpuSystem::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    auto cg = CurrentContainer->GetCgroup(CpuacctSubsystem);

    uint64_t val;
    error = CpuacctSubsystem.SystemUsage(cg, val);

    if (error) {
        L_ERR() << "Can't get system CPU usage: " << error << std::endl;
        value = "-1";
    } else {
        value = std::to_string(val);
    }

    return TError::Success();
}

class TContainerNetBytes : public TContainerProperty {
public:
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerNetBytes(std::string name, std::string desc)
                       : TContainerProperty(name, desc) {}
};

TError TContainerNetBytes::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::Bytes, m);

    return UintMapToString(m, value);
}

TError TContainerNetBytes::GetIndexed(const std::string &index,
                                      std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::Bytes, m);

    if (m.find(index) == m.end())
        return TError(EError::InvalidValue, "Invalid subscript for property");

    value = std::to_string(m[index]);

    return TError::Success();
}

class TContainerNetPackets : public TContainerProperty {
public:
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerNetPackets(std::string name, std::string desc)
                         : TContainerProperty(name, desc) {}
};

TError TContainerNetPackets::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::Packets, m);

    return UintMapToString(m, value);
}

TError TContainerNetPackets::GetIndexed(const std::string &index,
                                      std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::Packets, m);

    if (m.find(index) == m.end())
        return TError(EError::InvalidValue, "Invalid subscript for property");

    value = std::to_string(m[index]);

    return TError::Success();
}

class TContainerNetDrops : public TContainerProperty {
public:
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerNetDrops(std::string name, std::string desc)
                       : TContainerProperty(name, desc) {}
};

TError TContainerNetDrops::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::Drops, m);

    return UintMapToString(m, value);
}

TError TContainerNetDrops::GetIndexed(const std::string &index,
                                      std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::Drops, m);

    if (m.find(index) == m.end())
        return TError(EError::InvalidValue, "Invalid subscript for property");

    value = std::to_string(m[index]);

    return TError::Success();
}

class TContainerNetOverlimits : public TContainerProperty {
public:
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerNetOverlimits(std::string name, std::string desc)
                            : TContainerProperty(name, desc) {}
};

TError TContainerNetOverlimits::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::Overlimits, m);

    return UintMapToString(m, value);
}

TError TContainerNetOverlimits::GetIndexed(const std::string &index,
                                      std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::Overlimits, m);

    if (m.find(index) == m.end())
        return TError(EError::InvalidValue, "Invalid subscript for property");

    value = std::to_string(m[index]);

    return TError::Success();
}

class TContainerNetRxBytes : public TContainerProperty {
public:
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerNetRxBytes(std::string name, std::string desc)
                         : TContainerProperty(name, desc) {}
};

TError TContainerNetRxBytes::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::RxBytes, m);

    return UintMapToString(m, value);
}

TError TContainerNetRxBytes::GetIndexed(const std::string &index,
                                      std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::RxBytes, m);

    if (m.find(index) == m.end())
        return TError(EError::InvalidValue, "Invalid subscript for property");

    value = std::to_string(m[index]);

    return TError::Success();
}

class TContainerNetRxPackets : public TContainerProperty {
public:
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerNetRxPackets(std::string name, std::string desc)
                           : TContainerProperty(name, desc) {}
};

TError TContainerNetRxPackets::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::RxPackets, m);

    return UintMapToString(m, value);
}

TError TContainerNetRxPackets::GetIndexed(const std::string &index,
                                      std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::RxPackets, m);

    if (m.find(index) == m.end())
        return TError(EError::InvalidValue, "Invalid subscript for property");

    value = std::to_string(m[index]);

    return TError::Success();
}

class TContainerNetRxDrops : public TContainerProperty {
public:
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerNetRxDrops(std::string name, std::string desc)
                         : TContainerProperty(name, desc) {}
};

TError TContainerNetRxDrops::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::RxDrops, m);

    return UintMapToString(m, value);
}

TError TContainerNetRxDrops::GetIndexed(const std::string &index,
                                      std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    (void)CurrentContainer->GetStat(ETclassStat::RxDrops, m);

    if (m.find(index) == m.end())
        return TError(EError::InvalidValue, "Invalid subscript for property");

    value = std::to_string(m[index]);

    return TError::Success();
}

class TContainerIoRead : public TContainerProperty {
public:
    void Populate(TUintMap &m);
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerIoRead(std::string name, std::string desc)
                     : TContainerProperty(name, desc) {}
};

void TContainerIoRead::Populate(TUintMap &m) {
    auto memCg = CurrentContainer->GetCgroup(MemorySubsystem);
    auto blkCg = CurrentContainer->GetCgroup(BlkioSubsystem);
    TUintMap memStat;

    TError error = MemorySubsystem.Statistics(memCg, memStat);
    if (!error)
        m["fs"] = memStat["fs_io_bytes"] - memStat["fs_io_write_bytes"];

    std::vector<BlkioStat> blkStat;
    error = BlkioSubsystem.Statistics(blkCg, "blkio.io_service_bytes_recursive", blkStat);
    if (!error) {
        for (auto &s : blkStat)
            m[s.Device] = s.Read;
    }
}

TError TContainerIoRead::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    Populate(m);

    return UintMapToString(m, value);
}

TError TContainerIoRead::GetIndexed(const std::string &index,
                                    std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    Populate(m);

    if (m.find(index) == m.end())
        return TError(EError::InvalidValue, "Invalid subscript for property");

    value = std::to_string(m[index]);

    return TError::Success();
}

class TContainerIoWrite : public TContainerProperty {
public:
    void Populate(TUintMap &m);
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerIoWrite(std::string name, std::string desc)
                      : TContainerProperty(name, desc) {}
};

void TContainerIoWrite::Populate(TUintMap &m) {
    auto memCg = CurrentContainer->GetCgroup(MemorySubsystem);
    auto blkCg = CurrentContainer->GetCgroup(BlkioSubsystem);
    TUintMap memStat;

    TError error = MemorySubsystem.Statistics(memCg, memStat);
    if (!error)
        m["fs"] = memStat["fs_io_write_bytes"];

    std::vector<BlkioStat> blkStat;
    error = BlkioSubsystem.Statistics(blkCg, "blkio.io_service_bytes_recursive", blkStat);
    if (!error) {
        for (auto &s : blkStat)
            m[s.Device] = s.Write;
    }

}

TError TContainerIoWrite::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    Populate(m);

    return UintMapToString(m, value);
}

TError TContainerIoWrite::GetIndexed(const std::string &index, std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    Populate(m);

    if (m.find(index) == m.end())
        return TError(EError::InvalidValue, "Invalid subscript for property");

    value = std::to_string(m[index]);

    return TError::Success();
}

class TContainerIoOps : public TContainerProperty {
public:
    void Populate(TUintMap &m);
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerIoOps(std::string name, std::string desc)
                    : TContainerProperty(name, desc) {}
};

void TContainerIoOps::Populate(TUintMap &m) {
    auto memCg = CurrentContainer->GetCgroup(MemorySubsystem);
    auto blkCg = CurrentContainer->GetCgroup(BlkioSubsystem);
    TUintMap memStat;

    TError error = MemorySubsystem.Statistics(memCg, memStat);
    if (!error)
        m["fs"] = memStat["fs_io_operations"];

    std::vector<BlkioStat> blkStat;
    error = BlkioSubsystem.Statistics(blkCg, "blkio.io_service_bytes_recursive", blkStat);
    if (!error) {
        for (auto &s : blkStat)
            m[s.Device] = s.Read + s.Write;
    }
}

TError TContainerIoOps::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    Populate(m);

    return UintMapToString(m, value);
}

TError TContainerIoOps::GetIndexed(const std::string &index,
                                   std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    TUintMap m;
    Populate(m);

    if (m.find(index) == m.end())
        return TError(EError::InvalidValue, "Invalid subscript for property");

    value = std::to_string(m[index]);

    return TError::Success();
}

class TContainerTime : public TContainerProperty {
public:
    TError Get(std::string &value);
    TContainerTime(std::string name, std::string desc)
                   : TContainerProperty(name, desc) {}
};

TError TContainerTime::Get(std::string &value) {
    TError error = IsRunning();
    if (error)
        return error;

    if (CurrentContainer->IsRoot()) {
        struct sysinfo si;
        int ret = sysinfo(&si);
        if (ret)
            value = "-1";
        else
            value = std::to_string(si.uptime);

        return TError::Success();
    }

    // we started recording raw start/death time since porto v1.15;
    // in case we updated from old version, return zero
    if (!(CurrentContainer->PropMask & START_TIME_SET)) {
        CurrentContainer->StartTime = GetCurrentTimeMs();
        CurrentContainer->PropMask |= START_TIME_SET;
    }

    if (!(CurrentContainer->PropMask & DEATH_TIME_SET) &&
        (CurrentContainer->GetState() == EContainerState::Dead)) {

        CurrentContainer->DeathTime = GetCurrentTimeMs();
        CurrentContainer->PropMask |= DEATH_TIME_SET;
    }

    if (CurrentContainer->GetState() == EContainerState::Dead)
        value = std::to_string((CurrentContainer->DeathTime -
                               CurrentContainer->StartTime) / 1000);
    else
        value = std::to_string((GetCurrentTimeMs() -
                               CurrentContainer->StartTime) / 1000);

    return TError::Success();
}

class TContainerPortoStat : public TContainerProperty {
public:
    void Populate(TUintMap &m);
    TError Get(std::string &value);
    TError GetIndexed(const std::string &index, std::string &value);
    TContainerPortoStat(std::string name, std::string desc)
                        : TContainerProperty(name, desc, true) {}
};

void TContainerPortoStat::Populate(TUintMap &m) {
    m["spawned"] = Statistics->Spawned;
    m["errors"] = Statistics->Errors;
    m["warnings"] = Statistics->Warns;
    m["master_uptime"] = (GetCurrentTimeMs() - Statistics->MasterStarted) / 1000;
    m["slave_uptime"] = (GetCurrentTimeMs() - Statistics->SlaveStarted) / 1000;
    m["queued_statuses"] = Statistics->QueuedStatuses;
    m["queued_events"] = Statistics->QueuedEvents;
    m["created"] = Statistics->Created;
    m["remove_dead"] = Statistics->RemoveDead;
    m["slave_timeout_ms"] = Statistics->SlaveTimeoutMs;
    m["rotated"] = Statistics->Rotated;
    m["restore_failed"] = Statistics->RestoreFailed;
    m["started"] = Statistics->Started;
    m["running"] = CurrentContainer->GetRunningChildren();
    uint64_t usage = 0;
    auto cg = MemorySubsystem.Cgroup(PORTO_DAEMON_CGROUP);
    TError error = MemorySubsystem.Usage(cg, usage);
    if (error)
        L_ERR() << "Can't get memory usage of portod" << std::endl;
    m["memory_usage_mb"] = usage / 1024 / 1024;
    m["epoll_sources"] = Statistics->EpollSources;
    m["containers"] = Statistics->Containers;
    m["volumes"] = Statistics->Volumes;
    m["clients"] = Statistics->Clients;
}

TError TContainerPortoStat::Get(std::string &value) {
    TUintMap m;
    Populate(m);

    return UintMapToString(m, value);
}

TError TContainerPortoStat::GetIndexed(const std::string &index,
                                       std::string &value) {
    TUintMap m;
    Populate(m);

    if (m.find(index) == m.end())
        return TError(EError::InvalidValue, "Invalid subscript for property");

    value = std::to_string(m[index]);

    return TError::Success();
}

void InitContainerProperties(void) {
    ContainerPropMap[ContainerUser.Name] = &ContainerUser;
    ContainerPropMap[ContainerGroup.Name] = &ContainerGroup;
    ContainerMemoryGuarantee.Init();
    ContainerPropMap[ContainerMemoryGuarantee.Name] = &ContainerMemoryGuarantee;
    ContainerPropMap[ContainerCommand.Name] = &ContainerCommand;
    ContainerPropMap[ContainerVirtMode.Name] = &ContainerVirtMode;
    ContainerPropMap[ContainerCwd.Name] = &ContainerCwd;
    ContainerPropMap[ContainerStdinPath.Name] = &ContainerStdinPath;
    ContainerPropMap[ContainerStdoutPath.Name] = &ContainerStdoutPath;
    ContainerPropMap[ContainerStderrPath.Name] = &ContainerStderrPath;
    ContainerPropMap[ContainerIsolate.Name] = &ContainerIsolate;
    ContainerPropMap[ContainerBindDns.Name] = &ContainerBindDns;
    ContainerPropMap[ContainerRoot.Name] = &ContainerRoot;
    ContainerPropMap[ContainerNet.Name] = &ContainerNet;
    ContainerPropMap[ContainerHostname.Name] = &ContainerHostname;
    ContainerPropMap[ContainerRootRo.Name] = &ContainerRootRo;
    ContainerPropMap[ContainerEnv.Name] = &ContainerEnv;
    ContainerPropMap[ContainerBind.Name] = &ContainerBind;
    ContainerPropMap[ContainerIp.Name] = &ContainerIp;
    ContainerCapabilities.Init();
    ContainerPropMap[ContainerCapabilities.Name] = &ContainerCapabilities;
    ContainerPropMap[ContainerDefaultGw.Name] = &ContainerDefaultGw;
    ContainerPropMap[ContainerResolvConf.Name] = &ContainerResolvConf;
    ContainerPropMap[ContainerDevices.Name] = &ContainerDevices;
    ContainerPropMap[ContainerRawRootPid.Name] = &ContainerRawRootPid;
    ContainerPropMap[ContainerRawLoopDev.Name] = &ContainerRawLoopDev;
    ContainerPropMap[ContainerRawStartTime.Name] = &ContainerRawStartTime;
    ContainerPropMap[ContainerRawDeathTime.Name] = &ContainerRawDeathTime;
    ContainerPropMap[ContainerUlimit.Name] = &ContainerUlimit;
    ContainerPropMap[ContainerPortoNamespace.Name] = &ContainerPortoNamespace;
    ContainerPropMap[ContainerStdoutLimit.Name] = &ContainerStdoutLimit;
    ContainerPropMap[ContainerMemoryLimit.Name] = &ContainerMemoryLimit;
    ContainerAnonLimit.Init();
    ContainerPropMap[ContainerAnonLimit.Name] = &ContainerAnonLimit;
    ContainerDirtyLimit.Init();
    ContainerPropMap[ContainerDirtyLimit.Name] = &ContainerDirtyLimit;
    ContainerRechargeOnPgfault.Init();
    ContainerPropMap[ContainerRechargeOnPgfault.Name] = &ContainerRechargeOnPgfault;
    ContainerPropMap[ContainerCpuPolicy.Name] = &ContainerCpuPolicy;
    ContainerPropMap[ContainerCpuLimit.Name] = &ContainerCpuLimit;
    ContainerPropMap[ContainerCpuGuarantee.Name] = &ContainerCpuGuarantee;
    ContainerIoPolicy.Init();
    ContainerPropMap[ContainerIoPolicy.Name] = &ContainerIoPolicy;
    ContainerIoLimit.Init();
    ContainerPropMap[ContainerIoLimit.Name] = &ContainerIoLimit;
    ContainerIopsLimit.Init();
    ContainerPropMap[ContainerIopsLimit.Name] = &ContainerIopsLimit;
    ContainerPropMap[ContainerNetGuarantee.Name] = &ContainerNetGuarantee;
    ContainerPropMap[ContainerNetLimit.Name] = &ContainerNetLimit;
    ContainerPropMap[ContainerNetPriority.Name] = &ContainerNetPriority;
    ContainerPropMap[ContainerRespawn.Name] = &ContainerRespawn;
    ContainerPropMap[ContainerMaxRespawns.Name] = &ContainerMaxRespawns;
    ContainerPropMap[ContainerPrivate.Name] = &ContainerPrivate;
    ContainerPropMap[ContainerNetTos.Name] = &ContainerNetTos;
    ContainerPropMap[ContainerAgingTime.Name] = &ContainerAgingTime;
    ContainerPropMap[ContainerEnablePorto.Name] = &ContainerEnablePorto;
    ContainerPropMap[ContainerWeak.Name] = &ContainerWeak;
    ContainerPropMap[ContainerAbsoluteName.Name] = &ContainerAbsoluteName;
    ContainerPropMap[ContainerAbsoluteNamespace.Name] = &ContainerAbsoluteNamespace;
    ContainerMemTotalGuarantee.Init();
    ContainerPropMap[ContainerMemTotalGuarantee.Name] = &ContainerMemTotalGuarantee;
    ContainerPropMap[ContainerOomKilled.Name] = &ContainerOomKilled;
    ContainerPropMap[ContainerState.Name] = &ContainerState;
    ContainerPropMap[ContainerParent.Name] = &ContainerParent;
    ContainerPropMap[ContainerRespawnCount.Name] = &ContainerRespawnCount;
    ContainerPropMap[ContainerRootPid.Name] = &ContainerRootPid;
    ContainerPropMap[ContainerExitStatus.Name] = &ContainerExitStatus;
    ContainerPropMap[ContainerStartErrno.Name] = &ContainerStartErrno;
    ContainerPropMap[ContainerStdout.Name] = &ContainerStdout;
    ContainerPropMap[ContainerStdoutOffset.Name] = &ContainerStdoutOffset;
    ContainerPropMap[ContainerStderr.Name] = &ContainerStderr;
    ContainerPropMap[ContainerStderrOffset.Name] = &ContainerStderrOffset;
    ContainerPropMap[ContainerMemUsage.Name] = &ContainerMemUsage;
    ContainerPropMap[ContainerAnonUsage.Name] = &ContainerAnonUsage;
    ContainerPropMap[ContainerMinorFaults.Name] = &ContainerMinorFaults;
    ContainerPropMap[ContainerMajorFaults.Name] = &ContainerMajorFaults;
    ContainerMaxRss.Init();
    ContainerPropMap[ContainerMaxRss.Name] = &ContainerMaxRss;
    ContainerPropMap[ContainerCpuUsage.Name] = &ContainerCpuUsage;
    ContainerPropMap[ContainerCpuSystem.Name] = &ContainerCpuSystem;
    ContainerPropMap[ContainerNetBytes.Name] = &ContainerNetBytes;
    ContainerPropMap[ContainerNetPackets.Name] = &ContainerNetPackets;
    ContainerPropMap[ContainerNetDrops.Name] = &ContainerNetDrops;
    ContainerPropMap[ContainerNetOverlimits.Name] = &ContainerNetOverlimits;
    ContainerPropMap[ContainerNetRxBytes.Name] = &ContainerNetRxBytes;
    ContainerPropMap[ContainerNetRxPackets.Name] = &ContainerNetRxPackets;
    ContainerPropMap[ContainerNetRxDrops.Name] = &ContainerNetRxDrops;
    ContainerPropMap[ContainerIoRead.Name] = &ContainerIoRead;
    ContainerPropMap[ContainerIoWrite.Name] = &ContainerIoWrite;
    ContainerPropMap[ContainerIoOps.Name] = &ContainerIoOps;
    ContainerPropMap[ContainerTime.Name] = &ContainerTime;
    ContainerPropMap[ContainerPortoStat.Name] = &ContainerPortoStat;
}
