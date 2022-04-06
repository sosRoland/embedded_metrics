#define CATCH_CONFIG_ENABLE_BENCHMARKING

#include <iostream>
#include <sstream>
#include <vector>

#include "catch2.h"
#include "json.h"

#include <cw_emf.h>


std::vector<nlohmann::json> split_string_by_newline(const std::string& str)
{
    auto result = std::vector<nlohmann::json>{};
    auto ss = std::stringstream{str};

    for (std::string line; std::getline(ss, line, '\n');)
        result.push_back(nlohmann::json::parse(line));

    return result;
}


TEST_CASE("AWS Embedded Metrics Format", "[main]") {

    SECTION("Empty Logger") {
        using namespace  std::literals::chrono_literals;
        std::string buffer;
        {
            cw_emf::logger<"test_ns", cw_emf::metrics<>, cw_emf::dimensions<>, cw_emf::log_messages<>, cw_emf::output_sink_string> logger(buffer);
        }

        auto test_data = split_string_by_newline(buffer);
        REQUIRE(test_data.size() == 1);

        auto& emf_message = test_data.at(0);

        REQUIRE(emf_message.is_object());

        REQUIRE(emf_message.contains("_aws"));
        REQUIRE(emf_message["_aws"].is_object());

        REQUIRE(emf_message["_aws"].contains("Timestamp"));
        REQUIRE(emf_message["_aws"]["Timestamp"].is_number_integer());
        REQUIRE( (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch() - 1s)).count() <  emf_message["_aws"]["Timestamp"]);
        REQUIRE(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() >= emf_message["_aws"]["Timestamp"]);

        REQUIRE(emf_message["_aws"].contains("CloudWatchMetrics"));
        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"].is_array());
        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"].size() == 1);
        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0].is_object());

        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0].contains("Namespace"));
        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0]["Namespace"].is_string());
        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0]["Namespace"] == "test_ns");

        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0].contains("Dimensions"));
        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0]["Dimensions"].is_array());
        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0]["Dimensions"].size() == 1);
        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0]["Dimensions"][0].is_array());
        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0]["Dimensions"][0].size() == 0);
    }

    SECTION("Single Metric Logger") {
        std::string buffer;
        {
            cw_emf::logger<"test_ns",
                    cw_emf::metrics<
                            cw_emf::metric<"metric_1", Aws::CloudWatch::Model::StandardUnit::Count>>,
                    cw_emf::dimensions<>,
                    cw_emf::log_messages<>,
                    cw_emf::output_sink_string> logger(buffer);

            logger.put_metrics_value<"metric_1">(42);
        }

        auto test_data = split_string_by_newline(buffer);
        REQUIRE(test_data.size() == 1);

        auto& emf_message = test_data.at(0);

        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0].contains("Metrics"));
        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0]["Metrics"].is_array());
        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0]["Metrics"].size() == 1);
        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0]["Metrics"][0].is_object());

        auto& metric_header = emf_message["_aws"]["CloudWatchMetrics"][0]["Metrics"][0];

        REQUIRE(metric_header.contains("Name"));
        REQUIRE(metric_header.contains("Unit"));

        REQUIRE(metric_header["Name"] == "metric_1");
        REQUIRE(metric_header["Unit"] == "Count");

        REQUIRE(emf_message.contains("metric_1"));
        REQUIRE(emf_message["metric_1"].is_number());

        REQUIRE(42 == emf_message["metric_1"]);
    }

    SECTION("Multiple Metric Logger") {
        std::string buffer;
        {
            cw_emf::logger<"test_ns",
                    cw_emf::metrics<
                            cw_emf::metric<"metric_1", Aws::CloudWatch::Model::StandardUnit::Count>,
                            cw_emf::metric<"metric_2", Aws::CloudWatch::Model::StandardUnit::Count>>,
                    cw_emf::dimensions<>,
                    cw_emf::log_messages<>,
                    cw_emf::output_sink_string> logger(buffer);

            logger.put_metrics_value<"metric_1">(42);
            logger.put_metrics_value<"metric_2">(3.1415);
        }

        auto test_data = split_string_by_newline(buffer);
        REQUIRE(test_data.size() == 1);

        auto &emf_message = test_data.at(0);

        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0]["Metrics"].size() == 2);

        for (auto& metric_header: emf_message["_aws"]["CloudWatchMetrics"][0]["Metrics"]) {
            REQUIRE(emf_message.contains(metric_header["Name"]));

            if (metric_header["Name"] == "metric_1")
                REQUIRE(42 == emf_message["metric_1"]);
            else if (metric_header["Name"] == "metric_2")
                REQUIRE(3.1415 == emf_message["metric_2"]);
            else
                REQUIRE(false);
        }
    }

    SECTION("Dimensions Logger") {
        using namespace std::literals::chrono_literals;
        std::string buffer;
        {
            cw_emf::logger<"test_ns",
                    cw_emf::metrics<>,
                    cw_emf::dimensions<
                        cw_emf::dimension_fixed<"version", "$LATEST">,
                        cw_emf::dimension<"request_id">>,
                    cw_emf::log_messages<>,
                    cw_emf::output_sink_string> logger(buffer);

            logger.dimension_value<"request_id">("req_abs_123");
        }

        auto test_data = split_string_by_newline(buffer);
        REQUIRE(test_data.size() == 1);

        auto &emf_message = test_data.at(0);

        REQUIRE(emf_message["_aws"]["CloudWatchMetrics"][0]["Dimensions"][0].size() == 2);

        for(auto& dimension: emf_message["_aws"]["CloudWatchMetrics"][0]["Dimensions"][0]) {
            if (dimension == "version") {
                REQUIRE(emf_message.contains("version"));
                REQUIRE(emf_message["version"] == "$LATEST");
            } else if (dimension == "request_id") {
                REQUIRE(emf_message.contains("request_id"));
                REQUIRE(emf_message["request_id"] == "req_abs_123");
            } else
                REQUIRE(false);
        }
    }

    SECTION("Log Message Logger") {
        using namespace std::literals::chrono_literals;
        std::string buffer;
        {
            cw_emf::logger<"test_ns",
                    cw_emf::metrics<>,
                    cw_emf::dimensions<>,
                    cw_emf::log_messages<
                        cw_emf::log_message<"tracing">>,
                    cw_emf::output_sink_string> logger(buffer);

            logger.log_value<"tracing">("Hallo World");
        }

        auto test_data = split_string_by_newline(buffer);
        REQUIRE(test_data.size() == 1);

        auto &emf_message = test_data.at(0);

        REQUIRE(emf_message.contains("tracing"));
        REQUIRE(emf_message["tracing"].is_string());
        REQUIRE(emf_message["tracing"] == "Hallo World");
    }

    SECTION("Metrics with more than 100 values") {
        using namespace std::literals::chrono_literals;
        std::string buffer;
        {
            cw_emf::logger<"test_ns",
                    cw_emf::metrics<
                        cw_emf::metric<"metric_1", Aws::CloudWatch::Model::StandardUnit::Count>>,
                    cw_emf::dimensions<>,
                    cw_emf::log_messages<>,
                    cw_emf::output_sink_string> logger(buffer);

            for (int i=0; i < 150; ++i) {
                logger.put_metrics_value<0>(i);
            }
        }

        auto test_data = split_string_by_newline(buffer);
        REQUIRE(test_data.size() == 2);

        std::size_t total_values{0};
        for (auto &emf_message: test_data) {
            REQUIRE(emf_message.contains("metric_1"));
            REQUIRE(emf_message["metric_1"].is_array());

            REQUIRE(emf_message["metric_1"].size() <= 100);

            total_values += emf_message["metric_1"].size();
        }

        REQUIRE(total_values == 150);
    }

    SECTION("Metrics with more than 100 values and single value metric") {
        using namespace std::literals::chrono_literals;
        std::string buffer;
        {
            cw_emf::logger<"test_ns",
                    cw_emf::metrics<
                            cw_emf::metric<"metric_1", Aws::CloudWatch::Model::StandardUnit::Count>,
                            cw_emf::metric<"metric_2", Aws::CloudWatch::Model::StandardUnit::Count>>,
                    cw_emf::dimensions<>,
                    cw_emf::log_messages<>,
                    cw_emf::output_sink_string> logger(buffer);

            for (int i=0; i < 150; ++i) {
                logger.put_metrics_value<"metric_1">(i);
            }

            logger.put_metrics_value<"metric_2">(42);
        }

        auto test_data = split_string_by_newline(buffer);
        REQUIRE(test_data.size() == 2);

        std::size_t total_values{0};
        for (auto &emf_message: test_data) {

            for (auto& metric_header: emf_message["_aws"]["CloudWatchMetrics"][0]["Metrics"]) {
                REQUIRE(emf_message.contains(metric_header["Name"]));

                if (metric_header["Name"] == "metric_1") {
                    REQUIRE(emf_message["metric_1"].is_array());
                    REQUIRE(emf_message["metric_1"].size() <= 100);

                    total_values += emf_message["metric_1"].size();
                } else if (metric_header["Name"] == "metric_2") {
                    REQUIRE(emf_message.contains("metric_2"));
                    REQUIRE(42 == emf_message["metric_2"]);
                } else
                    REQUIRE(false);
            }
        }

        REQUIRE(total_values == 150);
    }


    //        std::cout << emf_message.dump(3) << "\n";
}

TEST_CASE("Create Metric Benchmark", "[benchmark]") {
    BENCHMARK("Single metric, no output") {

        cw_emf::logger<"test_ns",
                cw_emf::metrics<
                        cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>>,
                cw_emf::dimensions<
                        cw_emf::dimension<"version">,
                        cw_emf::dimension_fixed<"function_name", "my_lambda_fun">>,
                cw_emf::log_messages<>,
                cw_emf::output_sink_null> logger;

      logger.put_metrics_value<0>(34);
   };

    BENCHMARK("Metric By Name, no output") {
       cw_emf::logger<"test_ns",
               cw_emf::metrics<
                       cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count, int>,
                       cw_emf::metric<"transfer_speed", Aws::CloudWatch::Model::StandardUnit::Bytes_Second>>,
               cw_emf::dimensions<
                       cw_emf::dimension<"version">,
                       cw_emf::dimension_fixed<"function_name", "my_lambda_fun">>,
               cw_emf::log_messages<>,
               cw_emf::output_sink_null> logger;


       logger.put_metrics_value<"test_metric">(82);
       logger.put_metrics_value<"transfer_speed">(1047.456);
    };

    BENCHMARK("150 Metrics, no output") {
        cw_emf::logger<"test_ns",
                cw_emf::metrics<cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count, int>>,
                cw_emf::dimensions<>,
                cw_emf::log_messages<>,
                cw_emf::output_sink_null> logger;

          for (int i=0; i < 150; ++i) {
              logger.put_metrics_value<0>(i + 1);
          }
      };

    BENCHMARK("Single metric") {
        std::string buffer;
        cw_emf::logger<"test_ns",
                cw_emf::metrics<
                        cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>>,
                cw_emf::dimensions<
                        cw_emf::dimension<"version">,
                        cw_emf::dimension_fixed<"function_name", "my_lambda_fun">>,
                cw_emf::log_messages<>,
                cw_emf::output_sink_string> logger(buffer);

        logger.dimension_value<"version">("1.0.0");
        logger.put_metrics_value<0>(34);

        logger.flush();
        return buffer;
    };

    BENCHMARK("Metric By Name") {
        std::string buffer;
        cw_emf::logger<"test_ns",
                cw_emf::metrics<
                        cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count, int>,
                        cw_emf::metric<"transfer_speed", Aws::CloudWatch::Model::StandardUnit::Bytes_Second>>,
                cw_emf::dimensions<
                        cw_emf::dimension<"version">,
                        cw_emf::dimension_fixed<"function_name", "my_lambda_fun">>,
                cw_emf::log_messages<>,
                cw_emf::output_sink_string> logger(buffer);


        logger.dimension_value<"version">("1.0.0");

        logger.put_metrics_value<"test_metric">(82);
        logger.put_metrics_value<"transfer_speed">(1047.456);

        logger.flush();
        return buffer;
   };

    BENCHMARK("150 Metrics") {
        std::string buffer;

        cw_emf::logger<"test_ns",
                cw_emf::metrics<cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count, int>>,
                cw_emf::dimensions<>,
                cw_emf::log_messages<>,
                cw_emf::output_sink_string> logger(buffer);

        for (int i=0; i < 150; ++i) {
            logger.put_metrics_value<0>(i + 1);
        }

        logger.flush();
        return buffer;
    };
}