#include <Core/Modelica.h>
#include <SimCoreFactory/Policies/FactoryConfig.h>
//#include <SimCoreFactory/OMCFactory/OMCFactory.h>
#include <Core/SimulationSettings/Factory.h>
#include <Core/SimulationSettings/GlobalSettings.h>

#define BOOST_NO_WCHAR

#ifdef ANALYZATION_MODE
SettingsFactory::SettingsFactory(PATH libraries_path,PATH config_path,PATH modelicasystem_path)
  :StaticSolverSettingsOMCFactory<OMCFactory>(libraries_path,modelicasystem_path,config_path)
{
}
#else
SettingsFactory::SettingsFactory(PATH libraries_path,PATH config_path,PATH modelicasystem_path)
  :SolverSettingsPolicy(libraries_path,modelicasystem_path,config_path)
{
}
#endif

SettingsFactory::~SettingsFactory(void)
{
}

boost::shared_ptr<IGlobalSettings> SettingsFactory::createSolverGlobalSettings()
{
  _global_settings =  boost::shared_ptr<IGlobalSettings>(new GlobalSettings());
  loadGlobalSettings(_global_settings);
  return _global_settings;
}

boost::shared_ptr<ISolverSettings>  SettingsFactory::createSelectedSolverSettings()
{
  string solver_name = _global_settings->getSelectedSolver();
  _solver_settings = createSolverSettings(solver_name,_global_settings);
  return _solver_settings;
}

