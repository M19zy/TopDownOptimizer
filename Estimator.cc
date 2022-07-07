#include "Estimator.h"

#include "absl/flags/flag.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"

#include <cmath>
#include <exception>

using namespace operations_research;

namespace
{



    
} // namespace



double Estimator::operator()(std::string_view solveId)
{
    std::vector<double> fractionalCovers = LinearProgramming(solveId);

    double result = 1;
    for (int relIndex = 0; relIndex < mRelations.size(); relIndex++)
    {
        result *= std::pow((double)mRelations[relIndex].get().Length(), fractionalCovers[relIndex]);
    }

    return result;
}


std::vector<double> Estimator::LinearProgramming(absl::string_view solveId)
{
    MPSolver::OptimizationProblemType problemType;
    if (!MPSolver::ParseSolverType(solveId, &problemType))
    {
        throw std::invalid_argument("Unknown solver.");
    }

    if (!MPSolver::SupportsProblemType(problemType))
    {
        throw std::invalid_argument("Unsupported problem type.");
    }

    MPSolver solver("LP", problemType);

    const double infinity = solver.infinity();
    const size_t relationNum = mRelations.size();

    // x_i ranges between 0 and 1
    std::vector<MPVariable*> fractionalCovers(relationNum);
    for (size_t i = 0; i < relationNum; i++)
    {
        fractionalCovers[i] = solver.MakeNumVar(0.0, 1.0, mRelations[i].get().Name());
    }

    // target: minimize sum(x_i * logw_i) \geq 1
    MPObjective* const objective = solver.MutableObjective();
    std::vector<double> edgeLogW = EdgeLogWeight();
    for (auto& attr : mAttrs)
    {
        std::vector<size_t> relIndices = relIndicesWithAtttr(attr);
        for (size_t relIndex : relIndices)
        {
            const auto& coefficientVar = fractionalCovers[relIndex];
            objective->SetCoefficient(coefficientVar, edgeLogW[relIndex]);
            objective->SetMinimization();
        }
    }

    // Constraints
    // \sum x_i \geq 1
    std::vector<MPConstraint*> constraints(mAttrs.size());
    for (size_t i = 0; i < mAttrs.size(); i++)
    {
        std::vector<size_t> relIndices = relIndicesWithAtttr(mAttrs[i]);
        constraints[i] = solver.MakeRowConstraint(1.0, infinity);
        for (size_t j = 0; j < relIndices.size(); j++)
        {
            size_t relIndex = relIndices[j];
            constraints[i]->SetCoefficient(fractionalCovers[relIndex], 1.0);
        }
    }

    const MPSolver::ResultStatus resultStatus = solver.Solve();

    if (resultStatus != MPSolver::OPTIMAL)
    {
        // throw std::domain_error("Result Not optimal");
    }

    std::vector<double> result(mRelations.size());
    for (size_t i = 0; i < mRelations.size(); i++)
    {
        result[i] = fractionalCovers[i]->solution_value();
    }

    return result;
}


std::vector<double> Estimator::EdgeLogWeight() const
{
    std::vector<double> logW(mRelations.size());

    for (int i = 0; i < logW.size(); i++)
        logW[i] = std::log((double)mRelations[i].get().Length());

    return logW;
}


std::vector<size_t> Estimator::relIndicesWithAtttr(std::string_view attr)
{
    std::vector<size_t> relIndices;
    for (size_t relIndex = 0; relIndex < mRelations.size(); relIndex++)
    {
        if (mRelations[relIndex].get().ExistAttr(attr))
            relIndices.push_back(relIndex);
    }

    return relIndices;
}
