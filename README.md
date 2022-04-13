# AWS Embedded Metrics Format Libray for C++

## Intro

This library produces Amazon Web Services (AWS) [Embedded Metrics Format](https://docs.aws.amazon.com/AmazonCloudWatch/latest/monitoring/CloudWatch_Embedded_Metric_Format_Specification.html) (EMF) from a C++ program. It was primeraly designed to be used with the AWS Lambda C++ Custom runtime, however it can be adopted to use other means of delivering the Metrics to AWS Cloudwatch.

The initial development has some further details and can be read [here](http://www.switchedonsystems.com/emf_for_cpp/)

## How to use it

The basic concept is that you create and configure a logger at the beginning of your execution scope (your AWS Lambda callback) and it will flush the message(s) when leaving the scope:

```c++
cw_emf::logger<"my_namespace",
               cw_emf::metrics<
                  cw_emf::metric<"my_metric", Aws::CloudWatch::Model::StandardUnit::Count>>> logger;
                  
logger.put_metrics_value<"my_metric">(42);
```

This creates a logger with a metric 'my_metric' as a counter in the namesapce 'my_namespace' and sets the value of 42 to it.

For your program to link you will need to link against AWS SDK library 'monitoring' and needs to be compiled with a C++20 compiler.

## The Logger

The Logger class has the following signature to setup your metrics output:

```c++
template<internal::named emf_namespace,
            typename metrics,
            typename dimensions = dimensions<>,
            typename logs = log_messages<>,
            internal::emf_msg_sink_c sink_t=output_sink_stdout>
    class logger;
```

**Template Parameters:**

1. The char* literal for the namespace
2. The list of the metrics you wish to send to Cloudwatch
3. Optional dimension(s) (this is build time constraint to 9 dimensions)
4. Optional log messages that are not included in the metrics processing of Cloudwatch
5. The message sink that generates the metrics JSON and optional delivers it. This is defaulted to one that outputs to stdout

**Logger API**

Each of the 3 value types (Metrics, Dimensions and Log Messages) come in 2 different calls, one for indexed access and one for named access. During testing a 20ns (on a 1.8GHz Intel i7) execution difference was meassured (in favour of indexed access).

```c++
template<int index> void put_metrics_value(auto value);
template<internal::named name> void put_metrics_value(auto value);

template<int index> void dimension_value(const std::string& value);
template<internal::named name> void dimension_value(const std::string& value);

template<int index> void log_value(const std::string& value);
template<internal::named name> void log_value(const std::string& value);
```

## Metrics

Each metric requires two required template paramaters, the name and a metric unit from AWS SDK and a 3rd optional one for the data type which is defaulted to a double. If you use your own types there needs to be an implementation of `std::to_string` avaialble to write the value.

If more than one value is supplied to a given metric, the output will automatically convert to an array. If the array size exceeds 100 elements, an additional message will be created with the remaining values and will be seperated with a newline.

## Dimensions

There are two types available for the dimensions:

```c++
template<internal::named dimension_name>
    class dimension;

template<internal::named dimension_name, internal::named dimension_value>
    class dimension_fixed;
```

The plain `dimension` class requires a value to be set using one of the `dimension_value` methods of the logger class, while the `dimension_fixed` class has a fixed compile time value.

## Performance

The following benchmarks were produced on a Intel i7-8550U running at 1.8GHz:

```
benchmark name                       samples       iterations    estimated
                                     mean          low mean      high mean
                                     std dev       low std dev   high std dev
-------------------------------------------------------------------------------
Single metric, no output                       100          1079     2.2659 ms 
                                        20.8797 ns     19.905 ns    22.6416 ns 
                                        6.50723 ns    4.09507 ns    9.91486 ns 
                                                                               
Metric By Name, no output                      100           537     2.2554 ms 
                                        38.4363 ns    38.3095 ns    38.7512 ns 
                                       0.933092 ns   0.249941 ns    1.67354 ns 
                                                                               
150 Metrics, no output                         100            81     2.2923 ms 
                                        316.684 ns    292.312 ns    356.739 ns 
                                        157.148 ns    108.265 ns    223.278 ns 
                                                                               
Single metric                                  100            15      2.376 ms 
                                        1.54702 us    1.47015 us    1.72739 us 
                                        561.371 ns    287.326 ns    1.06544 us 
                                                                               
Multiple Metric By Name                        100            11     2.4706 ms 
                                        2.59051 us    2.43701 us     2.8005 us 
                                        908.726 ns    715.058 ns    1.14998 us 
                                                                               
150 Metrics                                    100             4     2.4604 ms 
                                        6.00233 us    5.78149 us    6.42213 us 
                                        1.50656 us    905.425 ns    2.33404 us 
```

The *no output* benchmarks use a null message sink that effectivly does nothing, while the others use a string message sink.

## Example

```c++
cw_emf::logger<"test_ns",
        cw_emf::metrics<
            cw_emf::metric<"metric_1", Aws::CloudWatch::Model::StandardUnit::Count>,
            cw_emf::metric<"metric_2", Aws::CloudWatch::Model::StandardUnit::Milliseconds>>,
        cw_emf::dimensions<
            cw_emf::dimension_fixed<"version", "$LATEST">,
            cw_emf::dimension<"request_id">>,
        cw_emf::log_messages<
            cw_emf::log_message<"debug">>> logger;

logger.dimension_value<"request_id">("req_123_abc");

logger.put_metrics_value<"metric_1">(42);
logger.put_metrics_value<"metric_2">(3.1415);

logger.log_value<"debug">("Hello World");

```

