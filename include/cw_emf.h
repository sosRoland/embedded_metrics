//
// Created by roland on 19/02/2022.
//

#ifndef BASE_CW_EMF_H
#define BASE_CW_EMF_H

#include <string_view>
#include <tuple>
#include <chrono>
#include <concepts>

#include <aws/monitoring/model/StandardUnit.h>

namespace cw_emf {
    namespace internal {

        template<int N> struct named {

            constexpr named(char const (&s)[N]) {
                std::copy_n(s, N, this->m_elems);
            }

            constexpr auto operator<=>(named const&) const = default;

            constexpr std::string_view name() const {
                return &m_elems[0];
            }


            /**
             * Contained Data
             */
            char m_elems[N];
        };
        template<int N> named(char const(&)[N])->named<N>;
        named(std::nullptr_t n) ->named<0>;

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

        template<typename S> concept emf_msg_sink_c = requires(S sink) {
            { sink.open_root_object() };
            { sink.close_root_object() };
            { sink.open_object() };
            { sink.open_object("field") };
            { sink.open_object(std::string_view("field")) };
            { sink.close_object() };

            { sink.open_array() };
            { sink.open_array("field") };
            { sink.close_array() };

            { sink.write_next_element() };

            { sink.write_value("field", std::string("Hallo World")) };
            { sink.write_value("field", std::string_view("Value")) };
            { sink.write_value("field", true) };
            { sink.write_value("field", false) };
            { sink.write_value("field", 1) };
            { sink.write_value("field", 1l) };
            { sink.write_value("field", 1ll) };
            { sink.write_value("field", 1.2f) };
            { sink.write_value("field", 1.2L) };

            { sink.write_value(std::string("Hallo World")) };
            { sink.write_value("Value") };
            { sink.write_value(true) };
            { sink.write_value(1) };
            { sink.write_value(1.2f) };

            { sink.generate() } -> std::same_as<bool>;

        };

    }

    template<internal::named metric_name, Aws::CloudWatch::Model::StandardUnit unit, typename value_type = double>
    class metric {

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

    template<internal::emf_metric_c... metrics_t>
    class metrics {
        static constexpr int block_size{100};

    public:
        template<int index> void put_value(auto value) {
            std::get<index>(m_metrics).put_value(value);
        }

        template<internal::named name> void put_value_by_name(auto&& value) {

            std::apply([&](metrics_t&... m) -> void {

                auto put_value_f = [&](auto&& m) {
                    if (m.name() == name.name())
                        m.put_value(value);
                };

                (put_value_f(m), ...);
            }, m_metrics);
        }

        static constexpr int size() {
            return sizeof...(metrics_t);
        }

        std::size_t max_array_value_size() const {
            std::size_t max=0;
            auto max_f = [&](auto&& m) {
                max = std::max(max, m.size());
            };

            std::apply([&](const metrics_t&... m) -> void {
                (max_f(m), ...);
            }, m_metrics);

            return max;
        }

        int num_blocks() const {
            return max_array_value_size() / block_size;
        }



        void write_header(internal::emf_msg_sink_c auto& sink) const {
            if constexpr(size() > 0) {
                sink.write_next_element();

                sink.open_array("Metrics");

                write_recursive_header(sink);

                sink.close_array();
            }
        }

        void write_values(internal::emf_msg_sink_c auto& sink, int block) const {
            if constexpr(size() > 0) {
                write_recursive_values(sink, block);
            }
        }

    private:
        std::tuple<metrics_t...> m_metrics;

        template<int index=0>
        void write_recursive_header(internal::emf_msg_sink_c auto& sink) const {
            sink.open_object();

            const auto& metric = std::get<index>(m_metrics);

            sink.write_value("Name", metric.name());
            sink.write_next_element();
            sink.write_value("Unit", metric.unit_name());

            sink.close_object();

            if constexpr(index < sizeof...(metrics_t) - 1) {
                sink.write_next_element();
                write_recursive_header<(index+1)>(sink);
            }
        }

        template<int index=0>
        void write_recursive_values(internal::emf_msg_sink_c auto& sink, int block) const {
            const auto& metric = std::get<index>(m_metrics);

            if (metric.size() == 1 && block == 0) {
                sink.write_next_element();
                sink.write_value(metric.name(), metric.value_at(0));
            } else if (metric.size() > 1) {
                std::size_t start_index = block * block_size;

                if (metric.size() > start_index) {
                    std::size_t end_index = (block+1) * block_size;
                    if (metric.size() < end_index)
                        end_index = metric.size();

                    sink.write_next_element();
                    sink.open_array(metric.name());

                    for (int i=start_index; i < end_index; ++i) {
                        if (i != start_index)
                            sink.write_next_element();
                        sink.write_value(metric.value_at(i));
                    }

                    sink.close_array();
                }
            }

            if constexpr(index < sizeof...(metrics_t) - 1) {
                write_recursive_values<(index+1)>(sink, block);
            }
        }
    };


    template<internal::named dimension_name>
    class dimension {
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

    template<internal::named dimension_name, internal::named dimension_value>
    class dimension_fixed {
    public:

        std::string_view name() const {
            return dimension_name.name();
        }

        void value(const std::string& value) {}

        std::string_view value() const {
            return dimension_value.name();
        }
    };

    template<internal::emf_dimension_c... dimensions_t>
    class dimensions {
    public:
        static_assert(sizeof...(dimensions_t) < 10, "AWS CloudWatch allows a maximum of 9 dimensions_t");

        template<int index> void value(const std::string& value) {
            std::get<index>(m_dimensions).value(value);
        }

        template<internal::named name> void value_by_name(const std::string& value) {
            std::apply([&](dimensions_t&... all_dimensions) {

                auto value_f = [&](auto&& dimension) -> void {
                    if (dimension.name() == name.name())
                        dimension.value(value);
                };
                (value_f(all_dimensions), ...);
            }, m_dimensions);
        }

        static constexpr int size() {
            return sizeof...(dimensions_t);
        }

        void write_header(internal::emf_msg_sink_c auto& sink) const {
            sink.write_next_element();

            sink.open_array("Dimensions");
            sink.open_array();
            if constexpr(size() > 0) {

                write_recursive_header(sink);
            }

            sink.close_array();
            sink.close_array();
        }

        void write_values(internal::emf_msg_sink_c auto& sink) const {
            if constexpr(sizeof...(dimensions_t) > 0) {
                sink.write_next_element();
                write_recursive_values(sink);
            }

        }

    private:
        std::tuple<dimensions_t...> m_dimensions;

        template<int index=0> void write_recursive_header(auto& sink) const {
            sink.write_value(std::get<index>(m_dimensions).name());

            if constexpr(index < sizeof...(dimensions_t) - 1) {
                sink.write_next_element();
                write_recursive_header<(index+1)>(sink);
            }
        }

        template<int index=0>
        void write_recursive_values(auto& sink) const {
            const auto& dimension = std::get<index>(m_dimensions);
            sink.write_value(dimension.name(), dimension.value());

            if constexpr(index < sizeof...(dimensions_t) - 1) {
                sink.write_next_element();
                write_recursive_values<(index+1)>(sink);
            }
        }
    };


    class output_sink_string {
    public:
        output_sink_string(std::string& out_buffer): m_buffer{out_buffer} {}

        void open_root_object() {
            open_object();
        }
        void close_root_object() {
            m_buffer += "}\n";
        }

        void open_object() {
            m_buffer += '{';
        }
        void open_object(std::string_view name) {
            m_buffer += '"';
            m_buffer += name.data();
            m_buffer += "\": {";

        }
        void close_object() {
            m_buffer += '}';
        }

        void open_array() {
            m_buffer += '[';
        }
        void open_array(std::string_view name) {
            m_buffer += '"';
            m_buffer += name.data();
            m_buffer += "\": [";
        }
        void close_array() {
            m_buffer += ']';
        }

        void write_next_element() {
            m_buffer += ',';
        }

        void write_value(std::string_view name, const std::string& value) {
            m_buffer += '"';
            m_buffer += name.data();
            m_buffer += "\":";
            write_value(value);
        }
        void write_value(std::string_view name, std::string_view value) {
            m_buffer += '"';
            m_buffer += name.data();
            m_buffer += "\":";
            write_value(value);
        }
        void write_value(std::string_view name, bool value) {
            m_buffer += '"';
            m_buffer += name.data();
            m_buffer += "\":";
            write_value(value);
        }
        void write_value(std::string_view name, std::integral auto value) {
            m_buffer += '"';
            m_buffer += name.data();
            m_buffer += "\":";
            write_value(value);
        }
        void write_value(std::string_view name, std::floating_point auto value) {
            m_buffer += '"';
            m_buffer += name.data();
            m_buffer += "\":";
            write_value(value);
        }

        void write_value(const std::string& value) {
            m_buffer += '"';
            m_buffer += value;
            m_buffer += '"';
        }
        void write_value(std::string_view value) {
            m_buffer += '"';
            m_buffer += value.data();
            m_buffer += '"';
        }
        void write_value(bool value) {
            if (value)
                m_buffer += "true";
            else
                m_buffer += "false";
        }
        void write_value(std::integral auto value) {
            m_buffer += std::to_string(value);
        }
        void write_value(std::floating_point auto value) {
            m_buffer += std::to_string(value);
        }

        void done() {}

        constexpr bool generate() const {
            return true;
        }

    private:
        std::string& m_buffer;
    };

    class output_sink_stdout: public output_sink_string {
    public:
        output_sink_stdout(): output_sink_string(m_buffer) {}

        void done() {
            std::fputs(m_buffer.c_str(), stdout);
            std::fflush(stdout);
        }
    private:
        std::string m_buffer;
    };

    class output_sink_null {
    public:
        void open_root_object() const {}
        void close_root_object() const {}
        void open_object() const {}
        void open_object(std::string_view name) const {}
        void close_object() const {}

        void open_array() const {}
        void open_array(std::string_view name) const {}
        void close_array() const {}

        void write_next_element() const {}

        void write_value(std::string_view, const std::string&) const {}
        void write_value(std::string_view, std::string_view) const {}
        void write_value(std::string_view, bool) const {}
        void write_value(std::string_view, std::integral auto) const {}
        void write_value(std::string_view, std::floating_point auto) const {}

        void write_value(const std::string&) const {}
        void write_value(std::string_view) const {}
        void write_value(bool) const {}
        void write_value(std::integral auto) const {}
        void write_value(std::floating_point auto) const {}

        void done() {}

        constexpr bool generate() const {
            return false;
        }

    };

    template<internal::named emf_namespace, typename metrics, typename dimensions = dimensions<>,  internal::emf_msg_sink_c sink_t=output_sink_stdout>
    class logger {

    public:
        logger(auto&&... args): m_sink(args...) {}
        ~logger() {
            if (m_sink.generate())
                write();
        }

        template<int index> void put_metrics_value(auto value) {
            m_metrics.template put_value<index>(value);
        }

        template<internal::named name> void put_metrics_value(auto value) {
            m_metrics.template put_value_by_name<name>(value);
        }

        template<int index> void dimension_value(const std::string& value) {
            m_dimensions.template value<index>(value);
        }

        template<internal::named name> void dimension_value_by_name(const std::string& value) {
            m_dimensions.template value_by_name<name>(value);
        }


        void flush() {
            write();
        }
    private:
        metrics m_metrics;
        dimensions m_dimensions;
        sink_t m_sink;

        void write() {

            for (int block=0; block <= m_metrics.num_blocks(); ++block) {
                m_sink.open_root_object();

                // Header
                m_sink.open_object("_aws");
                m_sink.write_value("Timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                m_sink.write_next_element();

                // CloudWatchMetrics Header
                m_sink.open_array("CloudWatchMetrics");
                m_sink.open_object();

                m_sink.write_value("Namespace", emf_namespace.name());
                m_dimensions.write_header(m_sink);
                m_metrics.write_header(m_sink);

                m_sink.close_object();
                m_sink.close_array();
                // Close Header
                m_sink.close_object();

                // Data Section
                if constexpr(metrics::size() > 0 || dimensions::size() > 0) {

                    m_dimensions.write_values(m_sink);
                    m_metrics.write_values(m_sink, block);

                }

                m_sink.close_root_object();

            }

            m_sink.done();
        }

    };


}


#endif //BASE_CW_EMF_H
