#define CATCH_CONFIG_ENABLE_BENCHMARKING

#include <iostream>

#include "catch2.h"

#include <cw_emf.h>


TEST_CASE("AWS Embedded Metrics Format", "[main]") {

    SECTION("Create Metric") {
        emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count> metric;

        REQUIRE(metric.name() == "test_metric");
        REQUIRE(metric.unit_name() == "Count");

        emf_metrics<emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>> metrics;
        metrics.put_value<0>(3.45);

        REQUIRE(metrics.max_array_value_size() == 1);

    }

    SECTION("Single Value Use Logger") {
        emf_logger<"test_ns", emf_metrics<emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>>> logger;
        logger.put_metrics_value<0>(34.456783);
    }

    SECTION("Multi Metrics Use Logger") {
        emf_logger<"test_ns",
                emf_metrics<
                        emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>,
                        emf_metric<"other_metric", Aws::CloudWatch::Model::StandardUnit::Bytes_Second, int>>> logger;
        logger.put_metrics_value<0>(34.456783);

        for (int i=0; i < 110; ++i) {
            logger.put_metrics_value<1>(1+i);
        }
    }


    SECTION("Use Logger") {
        emf_logger<"test_ns", emf_metrics<emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count, int>>> logger;

        for (int i=0; i < 110; ++i) {
            logger.put_metrics_value<0>(1+i);
        }
    }

    SECTION("Use Dimensions") {
        emf_logger<"test_ns", emf_metrics<emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>>, emf_dimensions<emf_dimension<"version">>> logger;
        logger.put_metrics_value<0>(456.45676);
        logger.dimension_value_by_name<"version">("1.0.0");
    }

    SECTION("Use Dimensions with static value") {
        std::string buffer;
         emf_logger<"test_ns",
                emf_metrics<
                        emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>>,
                emf_dimensions<
                        emf_dimension<"version">,
                        emf_static_dimension<"function_name", "my_lambda_fun">>, emf_string_sink> logger(buffer);
        logger.put_metrics_value<0>(456.45676);
        logger.dimension_value_by_name<"version">("1.0.0");

        logger.flush();
        std::cout << "Buffer Value? " << buffer << std::endl;
    }
}

TEST_CASE("Create Metric Benchmark", "[benchmark]") {
    BENCHMARK("Single metric, no output") {

        emf_logger<"test_ns",
              emf_metrics<
                      emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>>,
              emf_dimensions<
                      emf_dimension<"version">,
                      emf_static_dimension<"function_name", "my_lambda_fun">>, emf_null_sink> logger;

      logger.put_metrics_value<0>(34);
   };

    BENCHMARK("Metric By Name, no output") {
       emf_logger<"test_ns",
               emf_metrics<
                       emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count, int>,
                       emf_metric<"transfer_speed", Aws::CloudWatch::Model::StandardUnit::Bytes_Second>>,
               emf_dimensions<
                       emf_dimension<"version">,
                       emf_static_dimension<"function_name", "my_lambda_fun">>, emf_null_sink> logger;


       logger.metric_value_by_name<"test_metric">(82);
       logger.metric_value_by_name<"transfer_speed">(1047.456);
    };

    BENCHMARK("150 Metrics, no output") {
          emf_logger<"test_ns", emf_metrics<emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count, int>>, emf_dimensions<>, emf_null_sink> logger;

          for (int i=0; i < 110; ++i) {
              logger.put_metrics_value<0>(i + 1);
          }
      };

    BENCHMARK("Single metric") {
        std::string buffer;
        emf_logger<"test_ns",
                  emf_metrics<
                          emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count>>,
                  emf_dimensions<
                          emf_dimension<"version">,
                          emf_static_dimension<"function_name", "my_lambda_fun">>, emf_string_sink> logger(buffer);

          logger.put_metrics_value<0>(34);

          logger.flush();
          return buffer;
      };

    BENCHMARK("Metric By Name") {
        std::string buffer;
        emf_logger<"test_ns",
               emf_metrics<
                       emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count, int>,
                       emf_metric<"transfer_speed", Aws::CloudWatch::Model::StandardUnit::Bytes_Second>>,
               emf_dimensions<
                       emf_dimension<"version">,
                       emf_static_dimension<"function_name", "my_lambda_fun">>, emf_string_sink> logger(buffer);


       logger.metric_value_by_name<"test_metric">(82);
       logger.metric_value_by_name<"transfer_speed">(1047.456);

       logger.flush();
       return buffer;
   };

    BENCHMARK("150 Metrics") {
        std::string buffer;

        emf_logger<"test_ns", emf_metrics<emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count, int>>, emf_dimensions<>, emf_string_sink> logger(buffer);

        for (int i=0; i < 110; ++i) {
            logger.put_metrics_value<0>(i + 1);
        }

        logger.flush();
        std::fputs(buffer.c_str(), stdout);
        return buffer;
    };

    BENCHMARK("150 Metrics") {
         emf_logger<"test_ns", emf_metrics<emf_metric<"test_metric", Aws::CloudWatch::Model::StandardUnit::Count, int>>, emf_dimensions<>, emf_console_sink> logger;

         for (int i=0; i < 110; ++i) {
             logger.put_metrics_value<0>(i + 1);
         }
     };

}