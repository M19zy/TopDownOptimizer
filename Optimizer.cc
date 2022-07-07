#include "Optimizer.h"
#include "Estimator.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <numeric>
#include <queue>
#include <set>
#include <vector>


namespace
{

constexpr bool Debug = false;
    
} // namespace

std::unique_ptr<LTPlan> LTOptimizer::operator()()
{
    GVO.clear();
    // std::vector<size_t> relIndices(GloablData::GRelation.size());
    std::vector<size_t> relIndices(mRelations.size());
    std::iota(relIndices.begin(), relIndices.end(), 0);
    auto plan = GeneratePlan(relIndices, mAttrs);
    std::reverse(GVO.begin(), GVO.end());

    return plan;
}


std::unique_ptr<LTPlan> 
LTOptimizer::GeneratePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    throw std::runtime_error("Error");
    return nullptr;
}

// Guarantee that at least one attribute in any relation appears once in attrs
std::vector<std::pair<std::vector<size_t>, std::vector<std::string>>>
LTOptimizer::TrySpliteRelations(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    std::vector<int> attrGroupId(attrs.size());
    std::vector<int> relGroupId(relationIndices.size());
    std::iota(attrGroupId.begin(), attrGroupId.end(), 0);
    std::fill(relGroupId.begin(), relGroupId.end(), -1);

    auto FindParent = [&](int vid){
        size_t faId = vid;
        while (faId != attrGroupId[faId])
        {
            attrGroupId[faId] = attrGroupId[attrGroupId[faId]];
            faId = attrGroupId[faId];
        }
        return faId;
    };

    auto Union = [&](int a1, int a2){
        size_t p1 = FindParent(a1), p2 = FindParent(a2);
        attrGroupId[p1] = p2;
    };

    for (int relId = 0; relId < relationIndices.size(); relId++)
    {
        // auto& rel = GloablData::GRelation[relationIndices[relId]];
        auto& rel = mRelations[relationIndices[relId]].get();
        for (int attrId = 0; attrId < attrs.size(); attrId++)
        {
            auto& attr = attrs[attrId];
            if ( rel.ExistAttr(attr) )
            {
                if (relGroupId[relId] == -1)
                    relGroupId[relId] = attrId;
                else
                {
                    Union(relGroupId[relId], attrId);
                }
            }
        }
    }

    std::map<int, std::vector<size_t>> groupRelMap;
    for (size_t relId = 0; relId < relationIndices.size(); relId++)
    {
        if ( relGroupId[relId] == -1)
            continue;
        int faId = FindParent(relGroupId[relId]);
        if (groupRelMap.count(faId) == 0)
            groupRelMap[faId] = std::vector<size_t>{relationIndices[relId]};
        else
            groupRelMap[faId].push_back(relationIndices[relId]);
    }

    std::map<int, std::vector<std::string>> groupAttrMap;
    for (int attrId = 0; attrId < attrs.size(); attrId++)
    {
        int faId = FindParent(attrGroupId[attrId]);
        if (groupAttrMap.count(faId) == 0)
            groupAttrMap[faId] = std::vector<std::string>{attrs[attrId]};
        else
            groupAttrMap[faId].push_back(attrs[attrId]);
    }

    std::vector<std::pair<std::vector<size_t>, std::vector<std::string>>> relationGroups;
    for (auto& [id, relVec] : groupRelMap)
    {
        auto& attrVec = groupAttrMap[id];
        relationGroups.emplace_back(std::move(relVec), std::move(attrVec));
    }
    
    return relationGroups;
}

std::vector<std::pair<std::vector<RelationRef>, std::vector<std::string>>>
LTOptimizer::TrySpliteRelationsD(std::vector<RelationRef>& relations, std::vector<std::string>& attrs)
{
    std::vector<size_t> attrParents(attrs.size());
    std::iota(attrParents.begin(), attrParents.end(), 0);

    std::map<std::string, size_t> attrsMap;
    for (size_t attrIndex = 0; attrIndex < attrs.size(); attrIndex++)
        attrsMap[attrs[attrIndex]] = attrIndex;
    
    auto FindParent = [&](std::string a){
        size_t attrIndex = attrsMap[a];
        while (attrIndex != attrParents[attrIndex])
        {
            attrParents[attrIndex] = attrParents[attrParents[attrIndex]];
            attrIndex = attrParents[attrIndex];
        }
        return attrIndex;
    };

    auto Union = [&](std::string a1, std::string a2){
        size_t p1 = FindParent(a1), p2 = FindParent(a2);
        attrParents[p1] = p2;
    };

    std::vector<std::string> relParents(relations.size());
    for (size_t relIndex = 0; relIndex < relations.size(); relIndex++)
    {
        auto& relAttrs = relations[relIndex].get().Attrs();
        std::string baseAttr;
        for (auto& [relAttr, _] : relAttrs)
        {
            if (attrsMap.find(relAttr) != attrsMap.end())
            {
                if (baseAttr == "")
                {
                    baseAttr = relAttr;
                    relParents[relIndex] = relAttr;
                }
                else
                    Union(baseAttr, relAttr);
            }
        }
    }

    std::vector<std::pair<std::vector<RelationRef>, std::vector<std::string>>> relationGroups;

    {
        std::vector<int> relVisited(relations.size(), 0);
        std::vector<int> attrVisited(attrs.size(), 0);

        for (size_t attrIndex = 0; attrIndex < attrs.size(); attrIndex++)
        {
            if (attrVisited[attrIndex])
                continue;
            
            std::vector<std::string> subAttrs;
            for (size_t subAttrIndex = attrIndex; subAttrIndex < attrs.size(); subAttrIndex++)
            {
                if (FindParent(attrs[attrIndex]) == FindParent(attrs[subAttrIndex]))
                {
                    attrVisited[subAttrIndex] = 1;
                    subAttrs.push_back(attrs[subAttrIndex]);
                }
            }

            std::vector<RelationRef> subRelations;
            for (size_t relIndex = 0; relIndex < relations.size(); relIndex++)
            {
                if (relVisited[relIndex])
                    continue;
                
                relVisited[relIndex] = 1;

                auto& relAttrs = relations[relIndex].get().Attrs();
                for (auto& [relAttr, _] : relAttrs)
                {
                    if (FindParent(relAttr) == FindParent(subAttrs[0]))
                    {
                        subRelations.push_back(relations[relIndex]);
                        break;
                    }
                }
            }

            relationGroups.emplace_back(subRelations, subAttrs);
        }
    }

    return relationGroups;
}


std::unique_ptr<LTPlan> 
TopDownLTOptimizer::GeneratePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    /*
    std::cout << "Relation: ";
    for (auto relid : relationIndices)
        std::cout << mRelations[relid].get().Name() << ' ';
    std::cout << std::endl << "Attr: ";
    for (auto attr : attrs)
        std::cout << attr << ' ';
    std::cout << std::endl;
    */
    auto CartesianQueries= TrySpliteRelations(relationIndices, attrs);

    if ( CartesianQueries.size() == 1)
    {
        return GeneratePlanInternal(relationIndices, attrs);
    }
    else
    {
        auto plan = std::make_unique<CartesianLTPlan>();
        plan->cost = 1;
        for (auto& [subRelations, subAttrs] : CartesianQueries)
        {
            auto subPlan = GeneratePlanInternal(subRelations, subAttrs);
            plan->cost *= subPlan->cost;
            plan->AddSubPlan(std::move(subPlan));
        }
        return plan;
    }
}


std::unique_ptr<LTPlan> 
TopDownLTOptimizer::GeneratePlanInternal(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    if (attrs.size() == 1)
    {
        if constexpr (Debug)
        {
            std::cout << attrs[0] << " is WCO" << std::endl;
        }

        std::unique_ptr<LTPlan> plan = std::make_unique<GJLTPlan>();
        plan->AddAttr(attrs[0]);
        plan->cost = std::numeric_limits<double>::max();
        for (size_t i : relationIndices)
        {
            auto& rel = mRelations[i].get();
            if (rel.ExistAttr(attrs[0]))
            {
                if (mRelations[i].get().Length() < plan->cost)
                    plan->cost = mRelations[i].get().Length();
                plan->AddRelationIndex(i);
            }
        }

        if ( !mBinaryAttrs.count(attrs[0]) )
            GVO.push_back(attrs[0]);
        return plan;
    }

    std::vector<std::pair<std::vector<size_t>, std::vector<std::string>>> relationIndicesGroups;
    std::string targetAttr = attrs[0];
    double estimatedMinSize = std::numeric_limits<double>::max();

    std::vector<RelationRef> relations;
    for  (size_t i : relationIndices)
        relations.emplace_back(mRelations[i]);

    // std::cout << "Find the most attribute from " << attrs.size() << " attributes" << std::endl;
    // Find the most expensive attribute
    // bool suppressSplit = false;
    for (size_t attrIndex = 0; attrIndex < attrs.size(); attrIndex++)
    {
        // std::cout << "Currently " << attrs[attrIndex] << std::endl;
        std::vector<std::string> restAttrs = attrs;
        restAttrs.erase(std::remove(restAttrs.begin(), restAttrs.end(), attrs[attrIndex]), restAttrs.end());
        // std::swap(restAttrs[attrIndex], restAttrs.back());
        // restAttrs.pop_back();
        auto currentRelationGroups = TrySpliteRelations(relationIndices, restAttrs);

        double currentEstimatedSize = 0;
        if ( currentRelationGroups.size() == 1 )
        {
            Estimator estimator(relations, currentRelationGroups[0].second);
            currentEstimatedSize = estimator("GLOP");
        }
        // bool suppressAttrSplit = false;
        if ( currentRelationGroups.size() > 1 )
        {
            // std::cout << "   groups: " << currentRelationGroups.size() << std::endl;
            // double unsplitCost = 1;
            // for (auto& [subRelations, subAttrs] : currentRelationGroups)
            // {
            //     std::vector<RelationRef> curRelRefs;
            //     for (size_t i : subRelations)
            //         curRelRefs.emplace_back(mRelations[i]);
            //     Estimator estimator(curRelRefs, subAttrs);
            //     unsplitCost *= estimator("GLOP");
            // }

            // double splitCost = 0;
            for (auto& [subRelations, subAttrs] : currentRelationGroups)
            {
                // std::cout << "   rel:\n         ";
                // for (auto rel: subRelations)
                //   std::cout << rel.get().Name() << ' ';
                // std::cout << std::endl;
                subAttrs.push_back(attrs[attrIndex]);
                // auto subAttrsDup = subAttrs;
                std::vector<RelationRef> curRelRefs;
                for (size_t i : subRelations)
                    curRelRefs.emplace_back(mRelations[i]);
                Estimator estimator(curRelRefs, subAttrs);
                double curEstiCost = estimator("GLOP"); 
                currentEstimatedSize += curEstiCost + curEstiCost * std::log2f(curEstiCost);
            }
            // if (unsplitCost < splitCost)
            // {
            //     currentEstimatedSize = unsplitCost;
            //     suppressAttrSplit = true;
            // }
            // else
            // {
            //     currentEstimatedSize = splitCost;
            //     suppressAttrSplit = false;
            // }
        }
        else if (currentRelationGroups.size() == 0)
        {
            std::cout << "ERROR: Cannot split query!" << std::endl << "  ";
            for (size_t i : relationIndices)
                std::cout << mRelations[relationIndices[i]].get().Name() << ' ';
            std::cout << std::endl << "  ";
            for (auto i : attrs)
                std::cout << i << ' ';
            std::cout << std::endl;
            
            throw std::runtime_error("Illegal query!");
        }

        // std::cout << "Relation " << attrs[attrIndex] << " groups: " << currentRelationGroups.size() << std::endl;
        // std::cout << "  " << attrs[attrIndex] << " Cost: " << currentEstimatedSize;

        if (estimatedMinSize > currentEstimatedSize)
        {
            estimatedMinSize = currentEstimatedSize;
            targetAttr = attrs[attrIndex];
            relationIndicesGroups= currentRelationGroups;
            // suppressSplit = suppressAttrSplit;
        }
    }

    if constexpr (Debug)
    {
        std::cout << "Target attribute is " << targetAttr << std::endl;
    }
    if ( relationIndicesGroups.size() > 1)
    {
        mBinaryAttrs.insert(targetAttr);
        GVO.push_back(targetAttr);
    }
    if (mBinaryAttrs.count(targetAttr) == 0)
        GVO.push_back(targetAttr);

    std::unique_ptr<LTPlan> plan;
    double cost = 1;
    if (relationIndicesGroups.size() == 1)
    {
        if constexpr (Debug)
        {
            std::cout << targetAttr << " is WCO" << std::endl << "  joined relations: ";
            for (auto& relId : relationIndicesGroups[0].first) std::cout << relId << ' ';
            std::cout << std::endl;
        }

        plan = std::make_unique<GJLTPlan>();
        auto subPlan = GeneratePlanInternal(relationIndicesGroups[0].first, relationIndicesGroups[0].second);
        cost *= estimatedMinSize; // subPlan->cost;
        plan->AddSubPlan(std::move(subPlan));
    }
    else if ( relationIndicesGroups.size() > 1 )
    {
        /*
        if (suppressSplit)
        {
            if constexpr (Debug)
            {
                std::cout << "UnSplit " << targetAttr << std::endl << "  joined relations: ";
                for (auto& [subRelations, subAttrs] : relationIndicesGroups)
                {
                    for (auto& relId : subRelations) std::cout << relId << ' ';
                    std::cout << std::endl;
                }
            }
            plan = std::make_unique<GJLTPlan>();
            auto castplan = std::make_unique<CartesianLTPlan>();
            for (auto& [subRelations, subAttrs] : relationIndicesGroups)
            {
                auto subPlan = GeneratePlanInternal(relationIndices, subAttrs);
                cost *= subPlan->cost;
                castplan->AddSubPlan(std::move(subPlan));
            }
            plan->AddAttr(targetAttr);
            for (size_t relId : relationIndices)
                if (mRelations[relId].get().ExistAttr(targetAttr))
                    plan->AddRelationIndex(relId);
            plan->AddSubPlan(std::move(castplan));
        }
        else
        {*/
            if constexpr (Debug)
            {
                std::cout << "Split " << targetAttr << std::endl;
            }
            std::cout << "Split " << targetAttr << std::endl;
            plan = std::make_unique<BinaryLTPlan>();
            // std::cout << "   Next groups:\n";
            for (auto& [subRelations, subAttrs] : relationIndicesGroups)
            {
                // std::cout << "      rels: ";
                // for (auto rel : subRelations)
                // std::cout << rel.get().Name() << " ";
                // std::cout << std::endl;
                // subAttrs.push_back(targetAttr);
                auto subPlan = GeneratePlanInternal(subRelations, subAttrs);
                cost *= subPlan->cost;
                plan->AddSubPlan(std::move(subPlan));
            }
        // }
    }
    else
    {
        throw std::runtime_error("Invalid query!");
    }
    plan->cost = cost;
    plan->AddAttr(targetAttr);
    for (size_t i : relationIndices)
        if ( mRelations[i].get().ExistAttr(targetAttr) )
            plan->AddRelationIndex(i);


    return plan;
}


std::unique_ptr<PlanBase> 
TopDownLTOptimizer::MakePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    auto CartesianQueries= TrySpliteRelations(relationIndices, attrs);

    if ( CartesianQueries.size() == 1)
    {
        std::cout << "One query" << std::endl;
        return MakePlanIntern(relationIndices, attrs);
    }
    else
    {
        auto plan = std::make_unique<CartesianPlan>();
        plan->EstimateLength = 1;
        std::cout << CartesianQueries.size() << std::endl;
        for (auto& [subRelations, subAttrs] : CartesianQueries)
        {
            std::cout << "relation: ";
            for (auto& rel : subRelations) std::cout << rel << ' ';
            std::cout << std::endl << "attrs: ";
            for (auto& rel : subAttrs) std::cout << rel << ' ';
            std::cout << std::endl;
            auto subPlan = MakePlanIntern(subRelations, subAttrs);
            plan->EstimateLength *= subPlan->EstimateLength;
            plan->SubPlans.emplace_back(std::move(subPlan));
        }
        return plan;
    }
}


std::unique_ptr<PlanBase> 
TopDownLTOptimizer::MakePlanIntern(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    if (attrs.size() == 1)
    {
        std::unique_ptr<SingleAttrPlan> plan = std::make_unique<SingleAttrPlan>();
        plan->TargetAttr = attrs[0];
        plan->EstimateLength = std::numeric_limits<double>::max();
        for (size_t relId : relationIndices)
            if (plan->EstimateLength > GloablData::GRelation[relId].Length())
                plan->EstimateLength = GloablData::GRelation[relId].Length();
        
        for (size_t relId : relationIndices)
            plan->RelIdWithAttr.push_back(relId);

        if ( !mBinaryAttrs.count(attrs[0]) )
            GVO.push_back(attrs[0]);
        return plan;
    }

    // more than two attributes
    std::vector<std::pair<std::vector<size_t>, std::vector<std::string>>> relationIndicesGroups;
    std::string targetAttr = attrs[0];
    double estimatedMinSize = std::numeric_limits<double>::max();

    std::vector<RelationRef> relations;
    for  (size_t i : relationIndices)
        relations.emplace_back(GloablData::GRelation[i]);


    // Find the most expensive attribute
    for (size_t attrIndex = 0; attrIndex < attrs.size(); attrIndex++)
    {
        std::vector<std::string> restAttrs = attrs;
        restAttrs.erase(std::remove(restAttrs.begin(), restAttrs.end(), attrs[attrIndex]), restAttrs.end());


        auto currentRelationGroups = TrySpliteRelations(relationIndices, restAttrs);

        double currentEstimatedSize = 0;
        if ( currentRelationGroups.size() == 1 )
        {
            Estimator estimator(relations, currentRelationGroups[0].second);
            currentEstimatedSize = estimator("GLOP");
        }
        else if ( currentRelationGroups.size() > 1 )
        {
            for (auto& [subRelations, subAttrs] : currentRelationGroups)
            {
                subAttrs.push_back(attrs[attrIndex]);
                std::vector<RelationRef> curRelRefs;
                for (size_t i : subRelations)
                    curRelRefs.emplace_back(GloablData::GRelation[i]);
                Estimator estimator(curRelRefs, subAttrs);
                double workload = estimator("GLOP");
                currentEstimatedSize += workload + workload * std::log2f(workload);
            }
        }
        else
        {
            std::cout << "ERROR: Cannot split query!" << std::endl << "  ";
            for (size_t i : relationIndices)
                std::cout << GloablData::GRelation[relationIndices[i]].Name() << ' ';
            std::cout << std::endl << "  ";
            for (auto i : attrs)
                std::cout << i << ' ';
            std::cout << std::endl;
            
            throw std::runtime_error("Illegal query!");
        }

        if (estimatedMinSize > currentEstimatedSize)
        {
            estimatedMinSize = currentEstimatedSize;
            targetAttr = attrs[attrIndex];
            relationIndicesGroups= currentRelationGroups;
        }
    }


    if ( relationIndicesGroups.size() > 1 )
    {
        mBinaryAttrs.insert(targetAttr);
        GVO.push_back(targetAttr);
    }
    if (mBinaryAttrs.count(targetAttr) == 0)
        GVO.push_back(targetAttr);

    std::unique_ptr<SingleAttrPlan> plan;
    double cost = 1;
    if (relationIndicesGroups.size() == 1)
    {
        if constexpr (Debug)
        {
            std::cout << targetAttr << " is WCO" << std::endl;
        }

        plan = std::make_unique<SingleAttrPlan>();
        auto subPlan = MakePlanIntern(relationIndicesGroups[0].first, relationIndicesGroups[0].second);
        cost = subPlan->EstimateLength;
        plan->SubPlans.emplace_back(std::move(subPlan));
    }
    else if ( relationIndicesGroups.size() > 1 )
    {
        if constexpr (Debug)
        {
            std::cout << targetAttr << " is LoopJoin" << std::endl;
        }

        plan = std::make_unique<SingleAttrPlan>();
        for (auto& [subRelations, subAttrs] : relationIndicesGroups)
        {
            auto subPlan = MakePlanIntern(subRelations, subAttrs);
            cost *= subPlan->EstimateLength;
            plan->SubPlans.emplace_back(std::move(subPlan));
        }
    }
    else
    {
        throw std::runtime_error("Invalid query!");
    }
    plan->EstimateLength = cost;
    plan->TargetAttr = targetAttr;
    for (size_t i : relationIndices)
        if ( GloablData::GRelation[i].ExistAttr(targetAttr) )
            plan->RelIdWithAttr.push_back(i);


    return plan;

}



//================================================================================================

// DP optimizer

std::unique_ptr<LTPlan> 
DPLTOptimizer::GeneratePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    auto CartesianQueries= TrySpliteRelations(relationIndices, attrs);

    if ( CartesianQueries.size() == 1)
    {
        return GeneratePlanInternal(relationIndices, attrs);
    }
    else
    {
        auto plan = std::make_unique<CartesianLTPlan>();
        for (auto& [subRelations, subAttrs] : CartesianQueries)
        {
            plan->AddSubPlan(GeneratePlanInternal(subRelations, subAttrs));
        }
        return plan;
    }
}


template<>
struct std::hash<std::vector<std::string>>
{
    std::size_t operator()(const std::vector<std::string>& plan) const noexcept
    {
        size_t h = 0;
        for (size_t i = 0; i < plan.size(); i++)
            h ^= std::hash<std::string>{}(plan[i]);
        return h;
    }
};


std::unique_ptr<PlanBase> 
DPLTOptimizer::MakePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    auto CartesianQueries= TrySpliteRelations(relationIndices, attrs);

    if ( CartesianQueries.size() == 1)
    {
        return MakePlanIntern(relationIndices, attrs);
    }
    else
    {
        auto plan = std::make_unique<CartesianPlan>();
        for (auto& [subRelations, subAttrs] : CartesianQueries)
        {
            // plan->AddSubPlan(MakePlanIntern(subRelations, subAttrs));
            plan->SubPlans.emplace_back(MakePlanIntern(subRelations, subAttrs));
        }
        return plan;
    }
}



std::unique_ptr<PlanBase> 
DPLTOptimizer::MakePlanIntern(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    for (auto attr : attrs)
    {
        std::vector<size_t> relatedRelations;
        double estimatedCost = std::numeric_limits<double>::max();

        for (size_t relId : relationIndices)
        {
            if (GloablData::GRelation[relId].ExistAttr(attr))
            {
                relatedRelations.push_back(relId);
                if (estimatedCost > GloablData::GRelation[relId].Length())
                    estimatedCost = GloablData::GRelation[relId].Length();
            }
        }
        std::vector<std::string> attrVec = {attr};
        DPPlanOrder subOrder(attrVec, relatedRelations, estimatedCost);
        mPlanMapVec[1][subOrder.hashValue] = subOrder;
    }

    for (size_t k = 2; k <= attrs.size(); k++)
    {
        auto& k1PlanMap = mPlanMapVec[k-1];

        // k-1 to k
        for (auto& [hashKey, k1plan] : k1PlanMap)
        {
            // choose the next attribute
            for (auto& nextAttr : attrs)
            {
                if (k1plan.ExistAttr(nextAttr))
                    continue;

                std::vector<std::string> estimatedAttrs(k1plan.attrOrder);
                estimatedAttrs.push_back(nextAttr);
                std::vector<RelationRef> relations;
                std::vector<size_t> relationsInd;
                for (size_t relId : relationIndices)
                {
                    auto& rel = GloablData::GRelation[relId];
                    for (auto& att : estimatedAttrs)
                        if (rel.ExistAttr(att))
                        {
                            relations.push_back(rel);
                            relationsInd.push_back(relId);
                            break;
                        }
                }

                Estimator estimator(relations, estimatedAttrs);
                double estimatedIncCost = estimator("GLOP") + k1plan.cost;

                size_t hashValue = std::hash<std::vector<std::string>>{}(estimatedAttrs);

                if (mPlanMapVec[k].count(hashValue) == 0)
                {
                    mPlanMapVec[k][hashValue] = DPPlanOrder{estimatedAttrs, relationsInd, estimatedIncCost};
                    mPlanMapVec[k][hashValue].childPlanOrder.emplace_back(k1plan);
                    mPlanMapVec[k][hashValue].relatedRelationIndices = relationsInd;
                }
                else
                {
                    auto& kplan = mPlanMapVec[k][hashValue];
                    if (estimatedIncCost < kplan.cost)
                    {
                        kplan.cost = estimatedIncCost;
                        kplan.childPlanOrder[0] = k1plan;
                        kplan.hashValue = hashValue;
                        kplan.attrSet = {estimatedAttrs.begin(), estimatedAttrs.end()};
                        kplan.attrOrder = estimatedAttrs;
                        kplan.relatedRelationIndices = relationsInd;
                    }
                }
            }
        }
    }

    auto& [_, cheapNode] = *mPlanMapVec[attrs.size()].begin();
    for (auto& att : cheapNode.attrOrder)
        GVO.push_back(att);

    return SeqConvertToPlan(cheapNode);
}

std::unique_ptr<PlanBase>
DPLTOptimizer::SeqConvertToPlan(DPPlanOrder& order)
{
    if (order.attrOrder.size() > 1)
    {
        auto plan = std::make_unique<SingleAttrPlan>();
        plan->RelIdWithAttr = order.relatedRelationIndices;
        plan->TargetAttr = order.attrOrder.back();
        plan->EstimateLength = order.cost;

        for (auto& suborder : order.childPlanOrder)
            // plan->AddSubPlan(SeqConvertToPlan(suborder.get()));
            plan->SubPlans.emplace_back(SeqConvertToPlan(suborder.get()));

        return plan;
    }
    else if ( order.attrOrder.size() <= 1 )
    {
        auto plan = std::make_unique<SingleAttrPlan>();
        plan->RelIdWithAttr = order.relatedRelationIndices;
        plan->TargetAttr = order.attrOrder.back();
        plan->EstimateLength = order.cost;
        // plan.get()->GetRelationIndices() = order.relatedRelationIndices;
        // plan.get()->AddAttr(order.attrOrder.back());
        if (order.childPlanOrder.size() != 0)
            // plan.get()->AddSubPlan(SeqConvertToPlan(order.childPlanOrder[0].get()));
            plan->SubPlans.emplace_back(SeqConvertToPlan(order.childPlanOrder[0].get()));
        return plan;
    }
}


bool DPLTOptimizer::DPPlanOrder::ExistAttr(std::string& attr)
{
    return attrSet.count(attr) > 0;
}


std::unique_ptr<LTPlan> 
DPLTOptimizer::GeneratePlanInternal(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs, int level)
{
    // the dp strategy which http://www.vldb.org/pvldb/vol12/p1692-mhedhbi.pdf uses is designed for graph query, where a edge connects two vertices at most. So we need to modify it a bit

    GeneratePlanOrder(relationIndices, attrs);

    // convert attr seq to plan
    auto& [_, cheapNode] = *mPlanMapVec[attrs.size()].begin();

    // std::cout << "attrs:" << std::endl;
    // for (auto& att : cheapNode.attrOrder)
    //     std::cout << att << ' ';
    // std::cout << std::endl;

    // for (auto& att : cheapNode.attrOrder)
    //     GVO.push_back(att);
    GVO = cheapNode.attrOrder;
    std::reverse(GVO.begin(), GVO.end());

    return DPSeqConvertToPlan(cheapNode);
}

std::unique_ptr<LTPlan> DPLTOptimizer::DPSeqConvertToPlan(DPPlanOrder& order)
{
    if (order.attrOrder.size() > 1)
    {
        auto plan = std::make_unique<BinaryLTPlan>();
        plan->GetRelationIndices() = std::move(order.relatedRelationIndices);
        plan->AddAttr(order.attrOrder.back());

        for (auto& suborder : order.childPlanOrder)
            plan->AddSubPlan(DPSeqConvertToPlan(suborder.get()));

        return plan;
    }
    else if ( order.attrOrder.size() <= 1 )
    {
        auto plan = std::make_unique<GJLTPlan>();
        plan.get()->GetRelationIndices() = order.relatedRelationIndices;
        plan.get()->AddAttr(order.attrOrder.back());
        if (order.childPlanOrder.size() != 0)
            plan.get()->AddSubPlan(DPSeqConvertToPlan(order.childPlanOrder[0].get()));
        return plan;
    }
}

DPLTOptimizer::DPPlanOrder::DPPlanOrder(std::vector<std::string>& attrOrd, std::vector<size_t>& relIndices, double cst)
    : attrOrder(attrOrd), relatedRelationIndices(relIndices), cost(cst), attrSet(attrOrd.begin(), attrOrd.end())
{
    hashValue = std::hash<std::vector<std::string>>{}(attrOrd);
}

void DPLTOptimizer::GeneratePlanOrder(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    for (auto& v : attrs)
    {
        double cost = std::numeric_limits<double>::max();
        std::vector<size_t> relatedRelationInd;
        for (size_t relId : relationIndices)
        {
            auto& rel = mRelations[relId].get();
            if ( rel.ExistAttr(v))
            {
                relatedRelationInd.push_back(relId);
                if (cost > rel.Length())
                    cost = rel.Length();
            }
        }
        std::vector<std::string> seqV{v};
        DPPlanOrder order{seqV, relatedRelationInd, cost};
        mPlanMapVec[1][order.hashValue] = order;
    }


    for (size_t k = 2; k <= mAttrs.size(); k++)
    {
        auto& k1PlanMap = mPlanMapVec[k-1];

        // k-1 to k
        for (auto& [hashKey, k1plan] : k1PlanMap)
        {
            // choose the next attribute
            for (auto& nextAttr : mAttrs)
            {
                if (k1plan.ExistAttr(nextAttr))
                    continue;

                std::vector<std::string> estimatedAttrs(k1plan.attrOrder);
                estimatedAttrs.push_back(nextAttr);
                std::vector<RelationRef> relations;
                std::vector<size_t> relationsInd;
                for (size_t relId : relationIndices)
                {
                    auto& rel = mRelations[relId].get();
                    for (auto& att : estimatedAttrs)
                        if (rel.ExistAttr(att))
                        {
                            relations.push_back(rel);
                            relationsInd.push_back(relId);
                            break;
                        }
                }

                Estimator estimator(relations, estimatedAttrs);
                double estimatedIncCost = estimator("GLOP") + k1plan.cost;

                size_t hashValue = std::hash<std::vector<std::string>>{}(estimatedAttrs);

                if (mPlanMapVec[k].count(hashValue) == 0)
                {
                    mPlanMapVec[k][hashValue] = DPPlanOrder{estimatedAttrs, relationsInd, estimatedIncCost};
                    mPlanMapVec[k][hashValue].childPlanOrder.emplace_back(k1plan);
                    mPlanMapVec[k][hashValue].relatedRelationIndices = relationsInd;
                }
                else
                {
                    auto& kplan = mPlanMapVec[k][hashValue];
                    if (estimatedIncCost < kplan.cost)
                    {
                        kplan.cost = estimatedIncCost;
                        kplan.childPlanOrder[0] = k1plan;
                        kplan.hashValue = hashValue;
                        kplan.attrSet = {estimatedAttrs.begin(), estimatedAttrs.end()};
                        kplan.attrOrder = estimatedAttrs;
                        kplan.relatedRelationIndices = relationsInd;
                    }
                }
            }
        }

    }

}



void DP5LTOptimizer::GeneratePlanOrder(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    using OrderRef = std::reference_wrapper<DPPlanOrder>;
    auto cmp = [&](OrderRef p1, OrderRef p2){
        return p1.get().cost < p2.get().cost;
    };
    std::priority_queue<OrderRef, std::vector<OrderRef>, decltype(cmp)> pq(cmp);

    for (auto& v : attrs)
    {
        double cost = std::numeric_limits<double>::max();
        std::vector<size_t> relatedRelationInd;
        for (size_t relId : relationIndices)
        {
            auto& rel = mRelations[relId].get();
            if ( rel.ExistAttr(v))
            {
                relatedRelationInd.push_back(relId);
                if (cost > rel.Length())
                    cost = rel.Length();
            }
        }
        std::vector<std::string> seqV{v};
        DPPlanOrder order{seqV, relatedRelationInd, cost};
        mPlanMapVec[1][order.hashValue] = order;
    }


    {
        for (auto& [h, p] : mPlanMapVec[1])
            pq.push(p);

        std::map<size_t, DPPlanOrder> pMap;
        int remain = SavedNum;
        while (remain and !pq.empty())
        {
            auto p = pq.top();
            pMap.insert({p.get().hashValue, p.get()});
            pq.pop();
            remain--;
        }

        while(!pq.empty())
            pq.pop();
        mPlanMapVec[1] = pMap;
    }


    for (size_t k = 2; k <= attrs.size(); k++)
    {
        // std::cout << "Loop " << k << std::endl;
        auto& k1PlanMap = mPlanMapVec[k-1];

        // k-1 to k
        for (auto& [hashKey, k1plan] : k1PlanMap)
        {
            // choose the next attribute
            for (auto& nextAttr : attrs)
            {
                if (k1plan.ExistAttr(nextAttr))
                    continue;

                std::vector<std::string> estimatedAttrs(k1plan.attrOrder);
                estimatedAttrs.push_back(nextAttr);
                std::vector<RelationRef> relations;
                std::vector<size_t> relationsInd;
                for (size_t relId : relationIndices)
                {
                    auto& rel = mRelations[relId].get();
                    for (auto& att : estimatedAttrs)
                        if (rel.ExistAttr(att))
                        {
                            relations.push_back(rel);
                            relationsInd.push_back(relId);
                            break;
                        }
                }

                Estimator estimator(relations, estimatedAttrs);
                double estimatedIncCost = estimator("GLOP") + k1plan.cost;

                size_t hashValue = std::hash<std::vector<std::string>>{}(estimatedAttrs);

                if (mPlanMapVec[k].count(hashValue) == 0)
                {
                    mPlanMapVec[k][hashValue] = DPPlanOrder{estimatedAttrs, relationsInd, estimatedIncCost};
                    mPlanMapVec[k][hashValue].childPlanOrder.emplace_back(k1plan);
                    mPlanMapVec[k][hashValue].relatedRelationIndices = relationsInd;
                }
                else
                {
                    auto& kplan = mPlanMapVec[k][hashValue];
                    if (estimatedIncCost < kplan.cost)
                    {
                        kplan.cost = estimatedIncCost;
                        kplan.childPlanOrder[0] = k1plan;
                        kplan.hashValue = hashValue;
                        kplan.attrSet = {estimatedAttrs.begin(), estimatedAttrs.end()};
                        kplan.attrOrder = estimatedAttrs;
                        kplan.relatedRelationIndices = relationsInd;
                    }
                }
            }
        }

        {
            for (auto& [h, p] : mPlanMapVec[k])
                pq.push(p);

            std::map<size_t, DPPlanOrder> pMap;
            int remain = SavedNum;
            while (remain and !pq.empty())
            {
                auto p = pq.top();
                pMap.insert({p.get().hashValue, p.get()});
                pq.pop();
                remain--;
            }

            while(!pq.empty())
                pq.pop();
            mPlanMapVec[k] = pMap;
        }
    }
}


//=====================================================================


std::unique_ptr<LTPlan> 
EHLTOptimizer::GeneratePlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    auto CartesianQueries= TrySpliteRelations(relationIndices, attrs);

    if ( CartesianQueries.size() == 1)
    {
        return GeneratePlanInternal(relationIndices, attrs);
    }
    else
    {
        auto plan = std::make_unique<CartesianLTPlan>();
        for (auto& [subRelations, subAttrs] : CartesianQueries)
        {
            plan->AddSubPlan(GeneratePlanInternal(subRelations, subAttrs));
        }
        return plan;
    }
}

std::unique_ptr<LTPlan> 
EHLTOptimizer::GeneratePlanInternal(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    double estimiatedCost = std::numeric_limits<double>::max();
    size_t trackedRelId = std::numeric_limits<size_t>::max();

    for (size_t relId : relationIndices)
    {
        auto& targetRelation = mRelations[relId].get();

        // checks if target relation is the cut edge
        std::vector<size_t> remainRelIndices = relationIndices;
        remainRelIndices.erase(std::remove(remainRelIndices.begin(), remainRelIndices.end(), relId), remainRelIndices.end());

        auto relationGroups = TrySpliteRelations(remainRelIndices, attrs);
        
        // count valid groups
        int validGroupNum = 0;
        for (auto& [relIndices, atts] : relationGroups)
            if (relIndices.size() > 0)
                validGroupNum++;
        if (validGroupNum <= 1)
            continue;

        double currentEstimatedCost = 0;
        for (auto& [relIndices, atts] : relationGroups)
        {
            if (relIndices.size() == 0)
                continue;

            std::vector<RelationRef> relRefs;
            for (size_t rid : relIndices)
                relRefs.emplace_back(mRelations[rid]);
            Estimator estimator(relRefs, atts);
            currentEstimatedCost += estimator("GLOP");
        }

        if (estimiatedCost > currentEstimatedCost)
        {
            estimiatedCost = currentEstimatedCost;
            trackedRelId = relId;
        }
    }

    // convert groups to plan
    if (trackedRelId == std::numeric_limits<size_t>::max())
    {
        for (auto& att : attrs)
            GVO.push_back(att);
        return GenerateSeqPlan(relationIndices, attrs);
    }
    else
    {
        std::unique_ptr<EHPlan> plan = std::make_unique<EHPlan>();
        plan->mRelationId = trackedRelId;

        std::vector<size_t> remainRelIndices = relationIndices;
        remainRelIndices.erase(std::remove(remainRelIndices.begin(), remainRelIndices.end(), trackedRelId), remainRelIndices.end());
        auto relationGroups = TrySpliteRelations(remainRelIndices, attrs);

        for (auto& [relIdVec, attrVec] : relationGroups)
            if (relIdVec.size() == 0)
                for (auto& att : attrVec)
                {
                    GVO.push_back(att);
                }

        for (auto& [relIdVec, attrVec] : relationGroups)
        {
            if (relIdVec.size() == 0)
                continue;
            // plan->AddSubPlan(GenerateSeqPlan(relIdVec, attrVec));
            std::vector<std::string> joinAttrs;
            for (auto& att : attrVec)
                if (mRelations[trackedRelId].get().ExistAttr(att))
                    joinAttrs.push_back(att);
            plan->AddEHSubPlan(GenerateSeqPlan(relIdVec, attrVec), joinAttrs);
            // plan->mHashAttrs.emplace_back(attrVec);
            for (auto& att : attrVec)
            {
                GVO.push_back(att);
            }
        }

        return plan;
    }
}


std::unique_ptr<LTPlan>
EHLTOptimizer::GenerateSeqPlan(std::vector<size_t>& relationIndices, std::vector<std::string>& attrs)
{
    if (attrs.size() < 1)
        throw std::invalid_argument("Too few attributes");

    auto plan = std::make_unique<GJLTPlan>();
    plan->AddAttr(attrs[0]);
    for (size_t relId : relationIndices)
        if (mRelations[relId].get().ExistAttr(attrs[0]))
            plan->AddRelationIndex(relId);

    if (attrs.size() > 1)
    {
        std::vector<std::string> nextAttrs = {attrs.begin() + 1, attrs.end()};
        plan->AddSubPlan(GenerateSeqPlan(relationIndices, nextAttrs));
    }

    return plan;
}

// deprecated

std::unique_ptr<LTPlan>
BottomUpLTOptimizer::GeneratePlan(std::vector<size_t>& relations, std::vector<std::string>& attrs)
{
    std::vector<std::vector<RelationRef>> relationGroups;

    // Find the least cost attribute
    for (size_t attrIndex = 0; attrIndex < attrs.size(); attrIndex++)
    {
        std::vector<std::string> restAttrs = attrs;
        restAttrs.erase(std::remove(restAttrs.begin(), restAttrs.end(), attrs[attrIndex]), restAttrs.end());
        auto currentRelationGroups = TrySpliteRelations(relations, restAttrs);
    }

    return nullptr;
}

