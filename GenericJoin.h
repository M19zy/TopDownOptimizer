#pragma once

#include "Range.h"
#include "Relation.h"
#include "Plan.h"

#include <functional>
#include <string>
#include <vector>


class GenericJoin
{
public:
    GenericJoin(std::unique_ptr<LTPlan> plan, std::vector<Relation>&& relations, std::vector<std::string>&& attrs)
        : mPlan(std::move(plan)), mRelations(std::move(relations)), mAttrs(std::move(attrs))
    {}

    Relation operator()();

private:
    std::vector<size_t> SelectRelationIndices(std::string_view attr, bool expect);

    std::vector<AttributeRef<int>> 
    FetchAttributes(const std::vector<size_t>& relationIndices, std::string_view attr);

    inline size_t ShortestRangeIndex(RangeTuple tuple, const std::vector<size_t>& indices);

    RangeTable SingleAttrWCOJoin(RangeTableRef tableRef, std::vector<size_t>& relIndices, std::string attr, double cost);

    RangeTable SingleAttrLoopJoin(std::vector<RangeTableRef>& tableRefs, std::vector<size_t>& relIndices, std::string attr, double cost);

    RangeTable SingleAttrCartesianJoin(std::vector<RangeTableRef>& tableRefs, double cost);

    void PrintEqTable(RangeTable& rangeTable, const std::vector<std::string>& attrs);

    RangeTable Execute(std::unique_ptr<LTPlan> plan);

    RangeTable ExecuteMuti(std::unique_ptr<LTPlan> plan);

    RangeTable ExecuteSingle(std::unique_ptr<LTPlan> plan);

    RangeTable ExecuteEH(std::unique_ptr<LTPlan> plan);

    RangeTable ExecuteCartesian(std::unique_ptr<LTPlan> plan);


private:
    std::unique_ptr<LTPlan> mPlan;
    std::vector<Relation> mRelations;
    std::vector<std::string> mAttrs;
};


