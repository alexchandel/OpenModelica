#pragma once
#include "ISimVar.h"
#include <string>
using std::string;
/**
SimVar Klasse zum verwalten einer Double Variable
*/
class SimDouble : public ISimVar
{
public:
  virtual ~SimDouble() {};
  SimDouble(double value) { _value = value; }
  virtual string getName() { return _name; }
  virtual void setName(string name) { _name = name; }
  double& getValue() { return _value; }

private:
  string _name;
  double _value;
};