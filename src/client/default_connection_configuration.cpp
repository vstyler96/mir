/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "default_connection_configuration.h"

#include "display_configuration.h"
#include "rpc/make_rpc_channel.h"
#include "rpc/null_rpc_report.h"
#include "mir/logging/dumb_console_logger.h"
#include "mir/input/input_platform.h"
#include "mir/input/null_input_receiver_report.h"
#include "logging/rpc_report.h"
#include "logging/input_receiver_report.h"
#include "mir/logging/shared_library_prober_report.h"
#include "mir/logging/null_shared_library_prober_report.h"
#include "lttng/rpc_report.h"
#include "lttng/input_receiver_report.h"
#include "lttng/shared_library_prober_report.h"
#include "connection_surface_map.h"
#include "lifecycle_control.h"
#include "mir/shared_library.h"
#include "client_platform_factory.h"
#include "mir_event_distributor.h"
#include "mir/shared_library_prober.h"

namespace mcl = mir::client;

namespace
{
std::string const off_opt_val{"off"};
std::string const log_opt_val{"log"};
std::string const lttng_opt_val{"lttng"};
std::string const default_platform_lib{"libmirclientplatform.so"};
}

mcl::DefaultConnectionConfiguration::DefaultConnectionConfiguration(
    std::string const& socket_file)
    : socket_file{socket_file}
{
}

std::shared_ptr<mcl::ConnectionSurfaceMap>
mcl::DefaultConnectionConfiguration::the_surface_map()
{
    return surface_map([]
        {
            return std::make_shared<mcl::ConnectionSurfaceMap>();
        });
}

std::shared_ptr<google::protobuf::RpcChannel>
mcl::DefaultConnectionConfiguration::the_rpc_channel()
{
    return rpc_channel(
        [this]
        {
            return mcl::rpc::make_rpc_channel(
                the_socket_file(), the_surface_map(), the_display_configuration(), the_rpc_report(), the_lifecycle_control(), the_event_sink());
        });
}

std::shared_ptr<mir::logging::Logger>
mcl::DefaultConnectionConfiguration::the_logger()
{
    return logger(
        []
        {
            return std::make_shared<mir::logging::DumbConsoleLogger>();
        });
}

std::shared_ptr<mcl::ClientPlatformFactory>
mcl::DefaultConnectionConfiguration::the_client_platform_factory()
{
    return client_platform_factory(
        [this]
        {
            auto const create_client_platform_factory =
                the_platform_library()->load_function<mcl::CreateClientPlatformFactory>(
                    "create_client_platform_factory");

            return create_client_platform_factory();
        });
}

std::shared_ptr<mir::input::receiver::InputPlatform>
mcl::DefaultConnectionConfiguration::the_input_platform()
{
    return input_platform(
        [this]
        {
            return mir::input::receiver::InputPlatform::create(the_input_receiver_report());
        });
}

std::string
mcl::DefaultConnectionConfiguration::the_socket_file()
{
    return socket_file;
}

std::shared_ptr<mcl::rpc::RpcReport>
mcl::DefaultConnectionConfiguration::the_rpc_report()
{
    return rpc_report(
        [this] () -> std::shared_ptr<mcl::rpc::RpcReport>
        {
            auto val_raw = getenv("MIR_CLIENT_RPC_REPORT");
            std::string const val{val_raw ? val_raw : off_opt_val};

            if (val == log_opt_val)
                return std::make_shared<mcl::logging::RpcReport>(the_logger());
            else if (val == lttng_opt_val)
                return std::make_shared<mcl::lttng::RpcReport>();
            else
                return std::make_shared<mcl::rpc::NullRpcReport>();
        });
}

std::shared_ptr<mir::input::receiver::InputReceiverReport>
mcl::DefaultConnectionConfiguration::the_input_receiver_report()
{
    return input_receiver_report(
        [this] () -> std::shared_ptr<mir::input::receiver::InputReceiverReport>
        {
            auto val_raw = getenv("MIR_CLIENT_INPUT_RECEIVER_REPORT");
            std::string const val{val_raw ? val_raw : off_opt_val};

            if (val == log_opt_val)
                return std::make_shared<mcl::logging::InputReceiverReport>(the_logger());
            else if (val == lttng_opt_val)
                return std::make_shared<mcl::lttng::InputReceiverReport>();
            else
                return std::make_shared<mir::input::receiver::NullInputReceiverReport>();
        });
}

std::shared_ptr<mcl::DisplayConfiguration> mcl::DefaultConnectionConfiguration::the_display_configuration()
{
    return display_configuration(
        []
        {
            return std::make_shared<mcl::DisplayConfiguration>();
        });
}

std::shared_ptr<mcl::LifecycleControl> mcl::DefaultConnectionConfiguration::the_lifecycle_control()
{
    return lifecycle_control(
        []
        {
            return std::make_shared<mcl::LifecycleControl>();
        });
}

std::shared_ptr<mcl::EventSink> mcl::DefaultConnectionConfiguration::the_event_sink()
{
    return event_distributor(
        []
        {
            return std::make_shared<MirEventDistributor>();
        });
}

std::shared_ptr<mcl::EventHandlerRegister> mcl::DefaultConnectionConfiguration::the_event_handler_register()
{
    return event_distributor(
        []
        {
            return std::make_shared<MirEventDistributor>();
        });
}

std::shared_ptr<mir::SharedLibraryProberReport> mir::client::DefaultConnectionConfiguration::the_shared_library_prober_report()
{
    return shared_library_prober_report(
        [this] () -> std::shared_ptr<mir::SharedLibraryProberReport>
        {
            auto val_raw = getenv("MIR_CLIENT_SHARED_LIBRARY_PROBER_REPORT");
            std::string const val{val_raw ? val_raw : off_opt_val};
            if (val == log_opt_val)
                return std::make_shared<mir::logging::SharedLibraryProberReport>(the_logger());
            else if (val == lttng_opt_val)
                return std::make_shared<mcl::lttng::SharedLibraryProberReport>();
            else
                return std::make_shared<mir::logging::NullSharedLibraryProberReport>();
        });
}

std::shared_ptr<mir::SharedLibrary> mcl::DefaultConnectionConfiguration::the_platform_library()
{
    if (!platform_library)
    {
        auto const val_raw = getenv("MIR_CLIENT_PLATFORM_LIB");
        std::string const libname{val_raw ? val_raw : default_platform_lib};
        platform_library = std::make_shared<mir::SharedLibrary>(libname);
    }
    return platform_library;
}
