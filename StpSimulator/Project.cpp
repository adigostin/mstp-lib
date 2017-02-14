
#include "pch.h"
#include "SimulatorInterfaces.h"

using namespace std;

class Project : public IProject
{
};

extern const ProjectFactory projectFactory = [] { return unique_ptr<IProject>(new Project()); };
