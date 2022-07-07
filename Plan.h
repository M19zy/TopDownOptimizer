#pragma once

#include "Range.h"
#include <memory>
#include <string>
#include <vector>


class LTPlan
{
public:
    LTPlan() {}
    virtual ~LTPlan() {}

    virtual void AddSubPlan(std::unique_ptr<LTPlan> plan) {}

    virtual void AddAttr(std::string attr) {}

    virtual int SubPlanNum() const { return 0; }

    virtual std::string GetAttr() { return ""; }

    virtual std::unique_ptr<LTPlan> NextSubPlan() { return nullptr; }

    virtual bool IsEH() { return false; }

    void AddRelationIndex(size_t index) { mRelatedRelationIndices.push_back(index); }

    std::vector<size_t>& GetRelationIndices() { return mRelatedRelationIndices; }

public:
    double cost;

private:
    std::vector<size_t> mRelatedRelationIndices; // relations contiains selected attribute
};


// Subquery plan
class BinaryLTPlan : public LTPlan
{
public:
    BinaryLTPlan() {}

    virtual ~BinaryLTPlan() override {}

    virtual void AddSubPlan(std::unique_ptr<LTPlan> plan) override
    {
        mNextPlan.push_back(std::move(plan));
    }

    virtual void AddAttr(std::string attr) override
    {
        mAttr = attr;
    }

    virtual int SubPlanNum() const override { return mNextPlan.size(); }

    virtual std::unique_ptr<LTPlan> NextSubPlan() override
    {
        if (mNextPlan.empty())
            return nullptr;

        auto nextPlan = std::move(mNextPlan.back());
        mNextPlan.pop_back();

        return nextPlan;
    }

    virtual std::string GetAttr() override { return mAttr; }

private:
    std::string mAttr;
    std::vector<std::unique_ptr<LTPlan>> mNextPlan;
};

// Cartesian plan
class CartesianLTPlan : public LTPlan
{
public:
    CartesianLTPlan(): mAttr("") {}

    virtual ~CartesianLTPlan() override {}

    virtual void AddSubPlan(std::unique_ptr<LTPlan> plan) override
    {
        mNextPlan.push_back(std::move(plan));
    }

    virtual int SubPlanNum() const override { return mNextPlan.size(); }

    virtual std::unique_ptr<LTPlan> NextSubPlan() override
    {
        if (mNextPlan.empty())
            return nullptr;

        auto nextPlan = std::move(mNextPlan.back());
        mNextPlan.pop_back();

        return nextPlan;
    }

    virtual std::string GetAttr() override { return mAttr; }

private:
    std::string mAttr;
    std::vector<std::unique_ptr<LTPlan>> mNextPlan;
};

// EH plan
class EHPlan : public LTPlan
{
public:
    EHPlan() {}

    ~EHPlan() {}

    void AddEHSubPlan(std::unique_ptr<LTPlan> plan, std::vector<std::string>& attrs)
    {
        mNextPlan.emplace_back(std::move(plan), attrs);
    }

    virtual int SubPlanNum() const override { return mNextPlan.size(); }

    std::pair<std::unique_ptr<LTPlan>, std::vector<std::string>> 
    EHNextSubPlan()
    {
        auto nextPlan = std::move(mNextPlan.back());
        mNextPlan.pop_back();
        return nextPlan;
    }


    virtual bool IsEH() override { return true; }

public:
    size_t mRelationId;
    std::vector<std::pair<std::unique_ptr<LTPlan>, std::vector<std::string>>> mNextPlan;
    // std::vector<std::vector<std::string>> mHashAttrs;
    // std::vector<std::unique_ptr<LTPlan>> mNextPlan;
};

// Sequential plan
class GJLTPlan : public LTPlan
{
public:
    GJLTPlan(): mNextPlan(nullptr) {}

    virtual ~GJLTPlan() override {}

    virtual void AddSubPlan(std::unique_ptr<LTPlan> plan) override
    {
        mNextPlan = std::move(plan);
    }

    virtual void AddAttr(std::string attr) override
    {
        mAttr = attr;
    }

    virtual int SubPlanNum() const override { return mNextPlan == nullptr ? 0 : 1; }

    virtual std::unique_ptr<LTPlan> NextSubPlan() override
    {
        if (mNextPlan == nullptr)
            return nullptr;
        
        auto plan = std::move(mNextPlan);
        return plan;
    }

    virtual std::string GetAttr() override { return mAttr; }

private:
    std::string mAttr;
    std::unique_ptr<LTPlan> mNextPlan;
};


class GloablData
{
public:
    static std::vector<Relation> GRelation;
    static std::vector<std::string> GAttributes;
};

class PlanBase
{
public:
    PlanBase() {}

    virtual ~PlanBase() {}


    virtual std::unique_ptr<RangeTable> Execute() = 0;


    std::vector<std::unique_ptr<PlanBase>> SubPlans;
    double EstimateLength;
};

class SingleAttrPlan : public PlanBase
{
public:
    SingleAttrPlan() {}

    virtual ~SingleAttrPlan() override {}

    virtual std::unique_ptr<RangeTable> Execute();

    std::string TargetAttr;
    std::vector<size_t> RelIdWithAttr;

private:
    std::unique_ptr<RangeTable> ExeLeaf();

    std::unique_ptr<RangeTable> ExeNode();

    std::unique_ptr<RangeTable> ExeMulti();
};

class CartesianPlan : public PlanBase
{
public:
    CartesianPlan() {}

    virtual ~CartesianPlan() override {}

    virtual std::unique_ptr<RangeTable> Execute();
};

