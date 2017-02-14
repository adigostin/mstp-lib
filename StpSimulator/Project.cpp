
#include "pch.h"
#include "SimulatorInterfaces.h"

using namespace std;

class Project : public IProject
{
};

extern ProjectFactory* const projectFactory = [] { return unique_ptr<IProject>(new Project()); };
