/*
 * configurable.h
 *
 *  Created on: Nov 10, 2017
 *      Author: mhawari
 */

#ifndef COMMON_CONFIG_CONFIGURABLE_H_
#define COMMON_CONFIG_CONFIGURABLE_H_
#include <moduleconfiguration.h>
#include <log.h>
#include <unordered_map>
#include <tuple>
#include <utility>
#include <string>

template <typename T>
class PODWrapper {
private:
    T val;
public:
    operator T () const {return val;};
    PODWrapper(const T v) {val = v;};
    PODWrapper(){};
};
#define PROPERTIES_SUPPORTED_TYPES             \
    _(int, std::stoi)                  \
    _(double, std::stod)            \
    _(bool, !!std::stoi)              \
    _(float, std::stof)               \
    _(unsigned long, std::stol)

#define PROPERTY_REGISTER_OPTIONAL(name,member,defaultVal) registerProperty(this, name, &std::remove_reference<decltype(*this)>::type::member, defaultVal, true)

#define PROPERTY_REGISTER_MANDATORY(name,member,defaultVal) registerProperty(this, name, &std::remove_reference<decltype(*this)>::type::member, defaultVal, false)


template <typename U>
struct PropertyType {
    static U convert(const std::string &val){
        return U(val);
    }
};

#define _(i,j) template <> \
struct PropertyType<i> { \
    static i convert(const std::string &val){ \
        return j(val); \
    }; \
};
PROPERTIES_SUPPORTED_TYPES
#undef i

template<>
struct PropertyType<const char *> {
    static const char* convert(const std::string &val) {
        return val.c_str();
    };
};
template <typename T, typename U, typename V>
struct DefaultValueRetain {
    static V retain(const U &val) {
        return (V)val;
    };
};

template<typename T,typename U, typename V>
void registerProperty(T * pin, const std::string & name, U T::* member, const V& defaultVal,const bool optional) {
    if (!optional && !pin->getConfiguration()->_dynamicProperties.count(name))
    {
        LOG_ERROR("Pin Configuration Validation error: mandatory config \"%s\" not found!",name.c_str());

        abort();
    }
    try {
        if (optional && !pin->getConfiguration()->_dynamicProperties.count(name))
        {
            pin->*(member) = DefaultValueRetain<T, V, U>::retain(defaultVal);
            return;
        }
        pin->*(member) = PropertyType<U>::convert(pin->getConfiguration()->_dynamicProperties[name]);
    }
    catch (...) {
        LOG_ERROR("Exception catch when try to convert '%s'", name.c_str());
    }
}

#endif /* COMMON_CONFIG_CONFIGURABLE_H_ */
