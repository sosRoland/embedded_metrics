#define CATCH_CONFIG_ENABLE_BENCHMARKING

#include <iostream>

#include "catch2.h"

#include <cw_emf.h>


TEST_CASE("AWS Embedded Metrics Format", "[main]") {

    SECTION("Create Metric") {
        cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count> metric;

        REQUIRE(metric.name() == "test_metric");
        REQUIRE(metric.unit_name() == "Count");

        cw_emf::metrics<cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>> metrics;
        metrics.put_value<0>(3.45);

        REQUIRE(metrics.max_array_value_size() == 1);

    }

    SECTION("Single Value Use Logger") {
        cw_emf::logger<"test_ns", cw_emf::metrics<cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>>> logger;
        logger.put_metrics_value<0>(34.456783);
    }

    SECTION("Multi Metrics Use Logger") {
        cw_emf::logger<"test_ns",
                cw_emf::metrics<
                        cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>,
                        cw_emf::metric<"other_metric", Aws::CloudWatch::Model::StandardUnit::Bytes_Second, int>>> logger;
        logger.put_metrics_value<0>(34.456783);

        for (int i=0; i < 110; ++i) {
            logger.put_metrics_value<1>(1+i);
        }
    }


    SECTION("Logger with more then 150 metrics value") {
        cw_emf::logger<"test_ns", cw_emf::metrics<cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count, int>>> logger;

        for (int i=0; i < 110; ++i) {
            logger.put_metrics_value<0>(1+i);
        }
    }

    SECTION("Logger with more then 150 metrics value (multiple metrics)") {
        cw_emf::logger<"test_ns",
                cw_emf::metrics<cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count, int>,
                        cw_emf::metric<"xfer_rate", Aws::CloudWatch::Model::StandardUnit::Bytes_Second>>> logger;

        logger.put_metrics_value<"xfer_rate">(1.23);

        for (int i=0; i < 110; ++i) {
            logger.put_metrics_value<0>(1+i);
        }
    }


    SECTION("Use Dimensions") {
        cw_emf::logger<"test_ns", cw_emf::metrics<cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>>, cw_emf::dimensions<cw_emf::dimension<"version">>> logger;
        logger.put_metrics_value<0>(456.45676);
        logger.dimension_value<"version">("1.0.0");
    }

    SECTION("Use Dimensions with static value") {
        std::string buffer;
        cw_emf::logger<"test_ns",
                cw_emf::metrics<
                        cw_emf::metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>>,
                cw_emf::dimensions<
                        cw_emf::dimension<"version">,
                        cw_emf::dimension_fixed<"function_name", "my_lambda_fun">>,
                cw_emf::log_messages<cw_emf::log_message<"tracing">>,
                cw_emf::output_sink_string> logger(buffer);

        logger.put_metrics_value<0>(456.45676);
        logger.dimension_value<"version">("1.0.0");


        logger.log_value<"tracing">("Hallo World");

        logger.flush();
        std::cout << "Buffer Value? " << buffer << std::endl;
    }
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

          for (int i=0; i < 110; ++i) {
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

        for (int i=0; i < 110; ++i) {
            logger.put_metrics_value<0>(i + 1);
        }

        logger.flush();
        return buffer;
    };
}