#pragma once

#include "Relation.h"

#include "absl/strings/string_view.h"

#include <set>
#include <string>
#include <vector>


class Estimator
{
public:
    Estimator(const std::vector<RelationRef>& relations, const std::vector<std::string>& attrs)
        : mRelations(relations), mAttrs(attrs)
    {
    }

    double operator()(std::string_view solverId);

private:
    struct HyperEdge
    {
        int Id;
        size_t n;
        std::set<int> Vertices;

        bool ExistVertex(int vId) const
        {
            return Vertices.find(vId) != Vertices.end();
        }
    };

    std::vector<double> LinearProgramming(absl::string_view solverId);

    std::vector<double> EdgeLogWeight() const;

    std::vector<size_t> relIndicesWithAtttr(std::string_view attr);

private:
    const std::vector<RelationRef>& mRelations;
    const std::vector<std::string>& mAttrs;
};