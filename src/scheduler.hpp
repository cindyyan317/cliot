#pragma once

#include <crawler.hpp>
#include <reporting.hpp>
#include <runner.hpp>

#include <di.hpp>
#include <fmt/color.h>
#include <fmt/compile.h>
#include <inja/inja.hpp>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

template <
    typename ConnectionManagerType,
    typename RequestType,
    typename ResponseType,
    typename ReportRendererType>
class Scheduler {
    // all services needed for the scheduler:
    using env_t             = inja::Environment;
    using store_t           = inja::json;
    using con_man_t         = ConnectionManagerType;
    using reporting_t       = ReportEngine;
    using report_renderer_t = ReportRendererType;
    using crawler_t         = Crawler<RequestType, ResponseType, report_renderer_t>;
    using services_t        = di::Deps<env_t, store_t, con_man_t, reporting_t, report_renderer_t, crawler_t>;

    services_t services_;
    using flow_runner_t = FlowRunner<ConnectionManagerType, RequestType, ResponseType, report_renderer_t>;

public:
    Scheduler(services_t services)
        : services_{ services } { }

    void run() {
        auto const &reporting = services_.template get<reporting_t>();
        auto const &renderer  = services_.template get<report_renderer_t>();
        auto flows            = services_.template get<crawler_t>().get().crawl();
        for(auto const &[name, flow] : flows) {
            try {
                auto runner = flow_runner_t{
                    services_, name, flow.first, flow.second
                };
                runner.run();
                reporting.get().record(SimpleEvent{ "SUCCESS", name }, renderer);

            } catch(FlowException const &e) {
                reporting.get().record(FailureEvent{ name, e.path, e.issues, e.response }, renderer);
            }
        }
    }
};
