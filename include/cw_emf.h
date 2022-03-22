//
// Created by roland on 19/02/2022.
//

#ifndef BASE_CW_EMF_H
#define BASE_CW_EMF_H


#include <iostream>
#include <string_view>
#include <tuple>
#include <chrono>
#include <cmath>

#include <aws/monitoring/model/StandardUnit.h>

#include "named.h"
#include "json.h"


template<typename M> concept emf_metric_c = requires (M metric, typename M::type value){

    { metric.name() } -> std::same_as<std::string_view>;
    { metric.unit_name() } -> std::same_as<std::string>;

    { metric.put_value(value) };

    { metric.size() } -> std::same_as<std::size_t>;
};

template<typename D> concept emf_dimension_c = requires(D dimension) {

    { dimension.name() } -> std::same_as<std::string_view>;

    { dimension.value() } -> std::same_as<std::string_view>;
};

template<typename S> concept emf_msg_sink_c = requires(S sink, const nlohmann::json& data) {
    { sink.sink(data) };

    { sink.generate() } -> std::same_as<bool>;
};

template<named metric_name, Aws::CloudWatch::Model::StandardUnit unit, typename value_type = double>
class emf_metric {

public:
    using type = value_type;

    std::string_view name() const {
        return metric_name.name();
    }

    std::string unit_name() const {
        return Aws::CloudWatch::Model::StandardUnitMapper::GetNameForStandardUnit(unit);
    }

    void put_value(type value) {
        m_values.push_back(value);
    }

    type value_at(std::size_t index) const {
        return m_values.at(index);
    }

    constexpr std::size_t size() const {
        return m_values.size();
    }

private:
    std::vector<type> m_values;
};

template<emf_metric_c... metrics>
class emf_metrics {


public:
    template<int index> void put_value(auto value) {
        std::get<index>(m_metrics).put_value(value);
    }

    template<named name> void put_value_by_name(auto&& value) {

        std::apply([&](metrics&... m) -> void {

            auto put_value_f = [&](auto&& m) {
                if (m.name() == name.name())
                    m.put_value(value);
            };

            (put_value_f(m), ...);
        }, m_metrics);
    }

    constexpr std::size_t max_size() const {
        std::size_t max=0;
        auto max_f = [&](auto&& m) {
            max = std::max(max, m.size());
        };

        std::apply([&](const metrics&... m) -> void {
            (max_f(m), ...);
        }, m_metrics);

        return max;
    }


    void metrics_header(auto& header) const {
        header["Metrics"] = nlohmann::json::array();

        std::apply([&](const metrics&... m) -> void {
            auto header_f = [&](auto&& metric) {
                header["Metrics"].push_back(nlohmann::json::object({ {"Name", metric.name()} , {"Unit", metric.unit_name()}}));
            };

            (header_f(m), ...);
        }, m_metrics);
    }

    void metrics_values(auto& root, int block) const {

        std::apply([&](const metrics&... m) -> void {
            int upper_bound = ( (block+1)*100 < max_size() ? (block+1)*100 : max_size() );

            auto value_f = [&](const auto& metric) {

                if (metric.size() > 1) {
                    for (int index=(block*100); index < upper_bound; ++index) {
                        if (metric.size() < index)
                            return;

                        root[metric.name().data()].push_back( metric.value_at(index) );
                    }

                } else if (block == 0)
                    root[metric.name().data()] = metric.value_at(0);

            };

            (value_f(m), ...);
        }, m_metrics);
    }
private:
    std::tuple<metrics...> m_metrics;
};


template<named dimension_name>
class emf_dimension {
public:

    std::string_view name() const {
        return dimension_name.name();
    }

    void value(const std::string& value) {
        m_value = value;
    }

    std::string_view value() const {
        return m_value;
    }

private:
    std::string m_value;
};

template<named dimension_name, named dimension_value>
class emf_static_dimension {
public:

    std::string_view name() const {
        return dimension_name.name();
    }

    void value(const std::string& value) {}

    std::string_view value() const {
        return dimension_value.name();
    }
};

template<emf_dimension_c... dimensions>
class emf_dimensions {
public:
    static_assert(sizeof...(dimensions) < 10, "AWS CloudWatch allows a maximum of 9 dimensions");

    template<int index> void value(const std::string& value) {
        std::get<index>(m_dimensions).value(value);
    }

    template<named name> void value_by_name(const std::string& value) {
        std::apply([&](dimensions&... all_dimensions) {

            auto value_f = [&](auto&& dimension) -> void {
                if (dimension.name() == name.name())
                    dimension.value(value);
            };
            (value_f(all_dimensions), ...);
        }, m_dimensions);
    }

    void dimension_header(auto& header) const {
        if constexpr(sizeof...(dimensions) > 0) {
            header["Dimensions"] = nlohmann::json::array();
            nlohmann::json &dimension_array = header["Dimensions"].emplace_back();

            std::apply([&](const dimensions&... all_dimensions) {

                ([&](auto&& dimension) {
                    dimension_array.push_back(dimension.name());
                }(all_dimensions)  , ...);

            }, m_dimensions);
        }
    }

    void dimension_values(auto & root) const {
        if constexpr(sizeof...(dimensions) > 0) {
            std::apply([&](const dimensions&... all_dimensions) {

                ([&](auto&& dimension) {
                    root[dimension.name().data()] = dimension.value();
                }(all_dimensions)  , ...);

            }, m_dimensions);
        }
    }

private:
    std::tuple<dimensions...> m_dimensions;
};


class emf_console_sink {
public:
    void sink(const nlohmann::json& data) const {
        for (auto metrics_line: data) {
            std::cout << data.dump() << "\n";
        }
    }

    constexpr bool generate() const {
        return true;
    }
};

class emf_null_sink {
public:
    void sink(const nlohmann::json&) const {
        // NO-OP
    }

    constexpr bool generate() const {
        return false;
    }
};

template<named emf_namespace, typename metrics, typename dimensions = emf_dimensions<>,  emf_msg_sink_c sink=emf_console_sink>
class emf_logger {

public:
    ~emf_logger() {
        if (m_sink.generate())
            m_sink.sink(data());
    }

    template<int index> void put_metrics_value(auto value) {
        m_metrics.template put_value<index>(value);
    }

    template<named name> void metric_value_by_name(auto value) {
        m_metrics.template put_value_by_name<name>(value);
    }

    template<int index> void dimension_value(const std::string& value) {
        m_dimensions.template value<index>(value);
    }

    template<named name> void dimension_value_by_name(const std::string& value) {
        m_dimensions.template value_by_name<name>(value);
    }

    nlohmann::json data() const {
        nlohmann::json metrics_data = nlohmann::json::array();

        for (int block=0; block <= m_metrics.max_size() / 100; ++block) {
            auto& msg = metrics_data.template emplace_back(create_msg());

            m_metrics.metrics_header(msg["_aws"]["CloudWatchMetrics"][0]);
            m_dimensions.dimension_header(msg["_aws"]["CloudWatchMetrics"][0]);

            m_metrics.metrics_values(msg, block);
            m_dimensions.dimension_values(msg);
        }

        return metrics_data;
    }

    std::string str() const {
        std::string metrics_data;

        for (auto metrics_line: data()) {
            metrics_data += metrics_line.dump() + "\n";
        }

        return metrics_data;
    }

private:
    metrics m_metrics;
    dimensions m_dimensions;
    sink m_sink;

    nlohmann::json create_msg() const {
        nlohmann::json data;
        data["_aws"]["Timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        auto& metrics_header = data["_aws"]["CloudWatchMetrics"].emplace_back();
        metrics_header["Namespace"] = emf_namespace.name();

        return data;
    }
};


#endif //BASE_CW_EMF_H
