/*
 * pinfactory.h
 *
 *  Created on: Nov 7, 2017
 *      Author: mhawari
 */

#ifndef COMMON_PINS_PINFACTORY_H_
#define COMMON_PINS_PINFACTORY_H_

#include <unordered_map>
#include <string>
#include <functional>
#include <moduleconfiguration.h>

#define PIN_REGISTER(pinClass,pinName)                       \
template<>                                                   \
struct CPinClassInfo<pinClass>{                              \
    constexpr static const char * _name = pinName;           \
};                                                           \
static PinRegistration<pinClass> _pinRegistration_##pinClass;

class CIn;
class COut;

class CPinFactory
{
public:
    static CPinFactory* getInstance();
    void registerInputPinType(const char* pinType,
            std::function<CIn * (CModuleConfiguration*, int)> pinConstructor);
    void registerOutputPinType(const char* pinType,
            std::function<COut * (CModuleConfiguration*, int)> pinConstructor);
    CIn* createInputPin(const char* pinType, CModuleConfiguration* config,
            int id);
    COut* createOutputPin(const char* pinType, CModuleConfiguration* config,
            int id);

private:
    CPinFactory();
    static CPinFactory* _instance;
    std::unordered_map<std::string,
            std::function<CIn * (CModuleConfiguration*, int)> > _inputPins;
    std::unordered_map<std::string,
            std::function<COut * (CModuleConfiguration*, int)> > _outputPins;

};

template<typename T, bool = std::is_base_of<CIn, T>::value, bool =
        std::is_base_of<COut, T>::value>
class PinRegistration
{
    static_assert(std::is_same<T, T*>::value, "Not inherited from CIn or COut");
public:
    static bool registerPin();
    static const bool reg;
};

template<typename U>
struct CPinClassInfo
{
    static_assert(std::is_same<U, U*>::value, "No pin name registered");
};

/*Handle the case of CIn Pin*/
template<typename T>
class PinRegistration<T, true, false>
{
public:
    static bool registerPin()
    {
        CPinFactory::getInstance()->registerInputPinType(
                CPinClassInfo<T>::_name,
                [](CModuleConfiguration* config, int id)->CIn*
                {
                    return new T(config, id);
                });
        return true;
    }
    ;
    static bool reg;
    PinRegistration()
    {
        if (!reg)
        {
            reg = registerPin();
        }
    }
    ;
};

/*Handle the case of COut Pin*/
template<typename T>
class PinRegistration<T, false, true>
{
public:
    static bool registerPin()
    {
        CPinFactory::getInstance()->registerOutputPinType(
                CPinClassInfo<T>::_name,
                [](CModuleConfiguration* config, int id)->COut*
                {
                    return new T(config, id);
                });
        return true;
    }
    ;
    static bool reg;
    PinRegistration()
    {
        if (!reg)
        {
            reg = registerPin();
        }
    }
    ;

};

template<typename T>
bool PinRegistration<T,true,false>::reg;
template<typename T>
bool PinRegistration<T,false,true>::reg;

#endif /* COMMON_PINS_PINFACTORY_H_ */
