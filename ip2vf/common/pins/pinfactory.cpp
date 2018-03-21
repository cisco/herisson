/*
 * pinfactory.cpp
 *
 *  Created on: Nov 7, 2017
 *      Author: mhawari
 */

#include "log.h"
#include "pinfactory.h"

CPinFactory* CPinFactory::_instance = nullptr;
CPinFactory::CPinFactory()
{

}

CPinFactory* CPinFactory::getInstance()
{
    if (_instance)
        return _instance;
    _instance = new CPinFactory();
    return _instance;
}

void CPinFactory::registerInputPinType(const char* pinType,
        std::function<CIn * (CModuleConfiguration*, int)> pinConstructor)
{
    _inputPins[std::string(pinType)] = pinConstructor;
    LOG_INFO("register new input pin of type '%s'", pinType);
}

void CPinFactory::registerOutputPinType(const char* pinType,
        std::function<COut * (CModuleConfiguration*, int)> pinConstructor)
{
    _outputPins[std::string(pinType)] = pinConstructor;
    LOG_INFO("register new output pin of type '%s'", pinType);
}

CIn* CPinFactory::createInputPin(const char* pinType, CModuleConfiguration* config, int id)
{
    if (!_inputPins.count(std::string(pinType)))
        return NULL;
    return _inputPins[std::string(pinType)](config,id);
}

COut* CPinFactory::createOutputPin(const char* pinType, CModuleConfiguration* config, int id)
{
    if (!_outputPins.count(std::string(pinType)))
        return NULL;
    return _outputPins[std::string(pinType)](config,id);
}

