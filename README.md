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

Each of the 3 value types (Metrics, Dimensions and Log Messages) come in 2 different calls, one for indexed access and one for named access. During testing a about 20ns execution difference was meassured (in favour of indexed access).

```c++
template<int index> void put_metrics_value(auto value);
template<internal::named name> void put_metrics_value(auto value);

template<int index> void dimension_value(const std::string& value);
template<internal::named name> void dimension_value(const std::string& value);

template<int index> void log_value(const std::string& value);
template<internal::named name> void log_value(const std::string& value);
```

## Metrics

Each metric requires two template paramaters, the name and a metric unit from AWS SDK.
