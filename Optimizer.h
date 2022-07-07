#pragma once

#include "Plan.h"
#include "Relation.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <set>
// #include <span>


class LTOptimizer
{
public:
    LTOptimizer(std::vector<RelationRef>&& relations, std::vector<std::string>& attrs)
        : mRelations(std::move(relations)), mAttrs(attrs)
    {}

    LTOptimizer() {}

    virtual ~LTOptimizer() {}

    std::unique_ptr<LTPlan> operator()();

protected:
    virtual std::unique_ptr<LTPlan> 
    GeneratePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs);

    std::vector<std::pair<std::vector<RelationRef>, std::vector<std::string>>>
    TrySpliteRelationsD(std::vector<RelationRef>& relations, std::vector<std::string>& attrs);

    std::vector<std::pair<std::vector<size_t>, std::vector<std::string>>>
    TrySpliteRelations(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs);

    virtual std::unique_ptr<PlanBase> 
    MakePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs) {}

protected:

    std::vector<RelationRef> mRelations;
    std::vector<std::string> mAttrs;

public:
    std::vector<std::string> GVO;
    std::set<std::string> mBinaryAttrs;
};


// Top down
class TopDownLTOptimizer : public LTOptimizer
{
public:
    TopDownLTOptimizer(std::vector<RelationRef>&& relations, std::vector<std::string>& attrs)
        : LTOptimizer(std::move(relations), attrs)
    {}

    TopDownLTOptimizer() {}

    virtual ~TopDownLTOptimizer() {}

private:
    virtual std::unique_ptr<LTPlan> 
    GeneratePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs) override;

    std::unique_ptr<LTPlan> 
    GeneratePlanInternal(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs);

    virtual std::unique_ptr<PlanBase> 
    MakePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs) override;

    std::unique_ptr<PlanBase> 
    MakePlanIntern(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs);
};


// DP
class DPLTOptimizer : public LTOptimizer
{
public:
    DPLTOptimizer(): mPlanMapVec(GloablData::GAttributes.size() + 1) {}

    DPLTOptimizer(std::vector<RelationRef>&& relations, std::vector<std::string>& attrs)
        : LTOptimizer(std::move(relations), attrs), mPlanMapVec(attrs.size() + 1)
    {}

    virtual ~DPLTOptimizer() {}

protected:
    struct DPPlanOrder
    {
        DPPlanOrder() {}

        DPPlanOrder(const DPPlanOrder& order)
            : attrOrder(order.attrOrder), attrSet(order.attrSet), relatedRelationIndices(order.relatedRelationIndices), hashValue(order.hashValue), cost(order.cost), childPlanOrder(order.childPlanOrder)
        {}

        DPPlanOrder(std::vector<std::string>& attrOrder, std::vector<size_t>& relIndices, double cost);

        std::vector<std::string> attrOrder;
        std::set<std::string> attrSet;
        std::vector<size_t> relatedRelationIndices;
        size_t hashValue;
        double cost;
        std::vector<std::reference_wrapper<DPPlanOrder>> childPlanOrder;

        bool ExistAttr(std::string&);
    };

    virtual std::unique_ptr<LTPlan> 
    GeneratePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs) override;

    std::unique_ptr<LTPlan> 
    GeneratePlanInternal(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs, int level = 1);

    virtual void GeneratePlanOrder(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs);

    virtual std::unique_ptr<PlanBase> 
    MakePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs) override;

    std::unique_ptr<PlanBase> 
    MakePlanIntern(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs);

    std::unique_ptr<LTPlan>
    DPSeqConvertToPlan(DPPlanOrder& order);

    std::unique_ptr<PlanBase>
    SeqConvertToPlan(DPPlanOrder& order);

    std::vector<std::map<size_t, DPPlanOrder>> mPlanMapVec;
};


// DP5
class DP5LTOptimizer : public DPLTOptimizer
{
public:
    DP5LTOptimizer(std::vector<RelationRef>&& relations, std::vector<std::string>& attrs)
        : DPLTOptimizer(std::move(relations), attrs), SavedNum(5)
    {}

    virtual ~DP5LTOptimizer() {}

private:

    virtual void GeneratePlanOrder(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs) override;

    const int SavedNum;
};


// EH optimizer
class EHLTOptimizer : public LTOptimizer 
{
public:
    EHLTOptimizer(std::vector<RelationRef>&& relations, std::vector<std::string>& attrs)
        : LTOptimizer(std::move(relations), attrs)
    {}

    virtual ~EHLTOptimizer() {}

private:

    virtual std::unique_ptr<LTPlan> 
    GeneratePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs) override;

    std::unique_ptr<LTPlan> 
    GeneratePlanInternal(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs);

    std::unique_ptr<LTPlan>
    GenerateSeqPlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs);
};


// deprecated
class BottomUpLTOptimizer : public LTOptimizer
{
public:
    BottomUpLTOptimizer(std::vector<RelationRef>&& relations, std::vector<std::string>& attrs)
        : LTOptimizer(std::move(relations), attrs)
    {}

    virtual ~BottomUpLTOptimizer() {}

private:

    virtual std::unique_ptr<LTPlan> 
    GeneratePlan(std::vector<size_t>& relations, std::vector<std::string>& attrs) override;

};


