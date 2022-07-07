#include "GenericJoin.h"
#include "Range.h"
#include "Timer.h"

#include <iostream>
#include <limits>
#include <vector>


namespace
{

constexpr bool Debug = false;

RangeTable CreateEstimatedRangeTable(RangeTable& table, std::vector<size_t>& relIndices, size_t relTotalNum, double cost)
{
    size_t estimatedTuple = 0;
    for (size_t index = 0; index < table.Length(); index++)
    {
        RangeTuple tuple = table[index];

        size_t shortestLength = std::numeric_limits<size_t>::max();
        for (size_t relIndex : relIndices)
        {
            if (shortestLength > (tuple[relIndex].ed - tuple[relIndex].st))
                shortestLength = tuple[relIndex].ed - tuple[relIndex].st;
        }

        estimatedTuple += shortestLength;
    }

    estimatedTuple = std::min(estimatedTuple, (size_t)1e8);
    return RangeTable(relTotalNum, estimatedTuple);
}

RangeTable CreateFullEstimatedRangeTable(std::vector<RangeTableRef>& tableRefs, size_t relTotalNum, double cost)
{
    size_t estimatedTuples = 1;
    for (const auto& tableRef : tableRefs)
    {
        estimatedTuples *= tableRef.get().Length();
    }

    estimatedTuples = std::min(estimatedTuples, (size_t)1e8);

    return RangeTable(relTotalNum, estimatedTuples);
}



RangeTableIterator CreateRangeTableIter(const std::vector<RangeTableRef>& tableRefs)
{
    std::vector<size_t> tableLength(tableRefs.size());
    for (size_t i = 0; i < tableRefs.size(); i++)
        tableLength[i] = tableRefs[i].get().Length();
    
    return RangeTableIterator(tableLength);
}



void PrintRangeTableResult(std::vector<Relation>& relations, std::vector<std::string>& attributes, RangeTable& table)
{

}


}


std::vector<size_t> GenericJoin::SelectRelationIndices(std::string_view attr, bool expect)
{
    std::vector<size_t> indices;

    for (int index = 0; index < mRelations.size(); index++)
    {
        if (mRelations[index].ExistAttr(attr) == expect)
            indices.push_back(index);
    }

    return indices;
}


std::vector<AttributeRef<int>> 
GenericJoin::FetchAttributes(const std::vector<size_t>& relationIndices, std::string_view attr)
{
    std::vector<AttributeRef<int>> attrArray;

    for (auto index : relationIndices)
    {
        attrArray.push_back(mRelations[index][attr]);
    }

    return attrArray;
}


size_t GenericJoin::ShortestRangeIndex(RangeTuple tuple, const std::vector<size_t>& indices)
{
    size_t shortestRangeIndex = indices[0];
    for (auto index : indices)
    {
        if (tuple[index].Length() < tuple[shortestRangeIndex].Length())
            shortestRangeIndex = index;
    }

    return shortestRangeIndex;
}

void GenericJoin::PrintEqTable(RangeTable& rangeTable, const std::vector<std::string>& attrs)
{
    // for (const auto& attr : attrs)
    //     std::cout << attr << "  ";
    // std::cout << std::endl;

    for (size_t i = 0; i < rangeTable.Length(); i++)
    {
        RangeTuple tuple = rangeTable[i];
        for (size_t j = 0; j < attrs.size(); j++)
        {
            auto& attrName = attrs[j];
            for (auto& rel : mRelations)
            {
                if (rel.ExistAttr(attrName))
                {
                    const auto& attr = rel[attrName].get();
                    size_t index = tuple[j].st;
                    // std::cout << attr[index] << "  ";
                    break;
                }
            }
        }

        // std::cout << std::endl;
    }
}

RangeTable GenericJoin::ExecuteCartesian(std::unique_ptr<LTPlan> plan)
{
    std::vector<RangeTable> subRangeTables;
    for (size_t i = 0; i < plan->SubPlanNum(); i++)
    {
        auto subPlan = plan->NextSubPlan();
        subRangeTables.push_back(Execute(std::move(subPlan)));
    }

    std::vector<RangeTableRef> subRangeTableRefs{subRangeTables.begin(), subRangeTables.end()};

    return SingleAttrCartesianJoin(subRangeTableRefs, plan->cost);
}

RangeTable GenericJoin::ExecuteMuti(std::unique_ptr<LTPlan> plan)
{
    // std::cout << "  Executing muti" << std::endl;
    // std::cout << "    attr: " << plan->GetAttr() << std::endl;
    // std::cout << "    subplan num: " << plan->SubPlanNum() << std::endl;

    std::vector<RangeTable> subRangeTables;
    for (int i = plan->SubPlanNum() - 1; i >= 0; i--)
    {
        auto subPlan = plan->NextSubPlan();
        subRangeTables.push_back(Execute(std::move(subPlan)));
    }

    std::vector<RangeTableRef> subRangeTableRefs{subRangeTables.begin(), subRangeTables.end()};

    if (plan->GetAttr() != "")
        return SingleAttrLoopJoin(subRangeTableRefs, plan->GetRelationIndices(), plan->GetAttr(), plan->cost);
    else
        return SingleAttrCartesianJoin(subRangeTableRefs, plan->cost);
}

RangeTable GenericJoin::ExecuteSingle(std::unique_ptr<LTPlan> plan)
{
    // std::cout << "Executing Single" << std::endl;
    if (plan->SubPlanNum() == 0)
    {
        RangeTable rangeTable(mRelations.size(), 1);
        {
            RangeTuple tuple = rangeTable.AcquireTuple();
            for (int i = 0; i < mRelations.size(); i++)
            {
                tuple[i].st = 0;
                tuple[i].ed = mRelations[i].Length();
            }
        }

        auto res = SingleAttrWCOJoin(rangeTable, plan->GetRelationIndices(), plan->GetAttr(), plan->cost);
        if (false)
        {
        std::cout << "Attribute " << plan->GetAttr() << " result: " << std::endl;
        res.Print();
        }
        return res;
    }

    RangeTable subRangeTable = Execute(plan->NextSubPlan());

    return SingleAttrWCOJoin(subRangeTable, plan->GetRelationIndices(), plan->GetAttr(), plan->cost);
}

RangeTable GenericJoin::ExecuteEH(std::unique_ptr<LTPlan> plan)
{
    EHPlan* ehPlan = dynamic_cast<EHPlan*>(plan.get());

    std::vector<RangeTable> subRangeTables;
    std::vector<std::vector<std::string>> attrList;
    for (int i = ehPlan->SubPlanNum() - 1; i >= 0; i--)
    {
        auto&& [subPlan, atts] = ehPlan->EHNextSubPlan();
        attrList.emplace_back(atts);
        subRangeTables.push_back(Execute(std::move(subPlan)));
    }

    // sort each range table
    std::vector<std::vector<size_t>> sortIndicesVec;
    for (size_t i = 0; i < subRangeTables.size(); i++)
    {
        std::vector<size_t> mapRelationIndices;
        for (auto& attr : attrList[i])
        {
            for (auto& relId : subRangeTables[i].GetRelIndices())
                if (mRelations[relId].ExistAttr(attr))
                {
                    mapRelationIndices.push_back(relId);
                    break;
                }
        }

        sortIndicesVec.emplace_back(subRangeTables[i].LazySort(mRelations, mapRelationIndices, attrList[i]));
        // for(auto ind : sortIndicesVec.back())
        // {
            // RangeTuple tuple = subRangeTables[i][ind];
            // Range rng = tuple[mapRelationIndices[0]];
            // size_t st = rng.st;
            // std::cout << "Indi: " << ind << ' ' << mRelations[mapRelationIndices[i]][attrList[i][0]].get()[st];
            // std::cout << std::endl;
        // }
        // std::cout << std::endl;
    }

    // table: attribute to relation id
    std::vector<std::map<std::string, size_t>> tableAttr2RelIdMapVec;
    for (size_t tableId = 0; tableId < subRangeTables.size(); tableId++)
    {
        auto& table = subRangeTables[tableId];
        std::map<std::string, size_t> attr2RelIdMap;
        for (auto& att : attrList[tableId])
        {
            size_t targetRelId = 0;
            for (auto relId : table.GetRelIndices())
                if (mRelations[relId].ExistAttr(att))
                {
                    targetRelId = relId;
                    break;
                }
            attr2RelIdMap[att] = targetRelId;
            // std::cout << "attribute " << att << " tracked by relation " << targetRelId << std::endl;
        }
        tableAttr2RelIdMapVec.emplace_back(attr2RelIdMap);
    }

    // sort relation
    auto& joinRelation = mRelations[ehPlan->mRelationId];
    std::vector<std::string> attrSortSeq;
    {
        for (auto& atts : attrList)
            for (auto& att : atts)
                attrSortSeq.push_back(att);
        joinRelation.Sort(attrSortSeq);
    }

    std::vector<RangeTableRef> tableRefs;
    for (auto& table : subRangeTables)
        tableRefs.emplace_back(table);
    RangeTable nextRangeTable = CreateFullEstimatedRangeTable(tableRefs, mRelations.size(), plan->cost);
    std::vector<Range> rangeRange(subRangeTables.size());

    // merge
    size_t joinAttrNum = 0;
    for (auto& attVec : attrList)
        joinAttrNum += attVec.size();
    std::vector<int> preValue(joinAttrNum);
    std::fill(preValue.begin(), preValue.end(), -1);

    for (size_t jrTupleId = 0; jrTupleId < joinRelation.Length(); jrTupleId++)
    {
        std::vector<int> currentValue;
        for (auto& attVec : attrList)
            for (auto& att : attVec)
                currentValue.push_back(joinRelation[att].get()[jrTupleId]);
        {
            bool dup = true;
            for (size_t i = 0; i < joinAttrNum; i++)
                if (preValue[i] != currentValue[i])
                {
                    dup = false;
                    break;
                }
            if (dup)
                break;
        }
        // std::cout << "Current value: ";
        // for(int value : currentValue) std::cout << value << ' ';
        // std::cout << std::endl;

        bool valid = true;
        size_t joinAttrOff = 0;
        for (size_t tableId = 0; tableId < subRangeTables.size(); tableId++)
        {
            auto& table = subRangeTables[tableId];
            std::vector<int> targetValue(attrList[tableId].size());
            for (size_t i = 0; i < attrList[tableId].size(); i++)
                targetValue[i] = currentValue[i + joinAttrOff];
            joinAttrOff += attrList[tableId].size();

            // std::cout << "target value: ";
            // for (int value : targetValue) std::cout << value << ' ';
            // std::cout << std::endl;

            // table->rtupleId->attId
            auto converter = [&](size_t rtupleId, size_t relId, std::string& attr){
                size_t st = table[rtupleId][relId].st;
                int storeValue = this->mRelations[relId][attr].get()[st];
                return storeValue;
            };

            // std::cout << "order seq: " << std::endl;
            // for (int value : sortIndicesVec[tableId]) std::cout << value << ' ';
            // std::cout << std::endl;
            // std::string testattr = attrList[tableId][0];
            // size_t testRelId = tableAttr2RelIdMapVec[tableId][testattr];
            // std::cout << "test converter: " << std::endl;
            // for (size_t i = 0; i < table.Length(); i++)
            //     std::cout << converter(sortIndicesVec[tableId][i], testRelId, testattr) << ' ';
            // std::cout << std::endl;
            
            auto finderLow = [&](size_t tupleId, const std::vector<int>& valueTuple){
                RangeTuple rangeTuple = table[tupleId];
                for (size_t i = 0; i < attrList[tableId].size(); i++)
                {
                    auto& attr = attrList[tableId][i];
                    size_t relId = tableAttr2RelIdMapVec[tableId][attr];
                    int storeValue = converter(tupleId, relId, attr);
                    if (storeValue != valueTuple[i])
                        return storeValue < valueTuple[i];
                }
                return false;
            };

            auto finderUp = [&](const std::vector<int>& valueTuple, size_t tupleId){
                for (size_t i = 0; i < attrList[tableId].size(); i++)
                {
                    auto& attr = attrList[tableId][i];
                    size_t relId = tableAttr2RelIdMapVec[tableId][attr];
                    int storeValue = converter(tupleId, relId, attr);
                    if (storeValue != valueTuple[i])
                        return storeValue > valueTuple[i];
                }
                return false;
            };

            auto& sortIndices = sortIndicesVec[tableId];
            auto lowerIndex = std::lower_bound(sortIndices.begin(), sortIndices.end(), targetValue, finderLow) - sortIndices.begin();
            auto upperIndex = std::upper_bound(sortIndices.begin(), sortIndices.end(), targetValue, finderUp) - sortIndices.begin();

            if (lowerIndex >= upperIndex)
            {
                valid = false;
                break;
            }

            rangeRange[tableId].st = lowerIndex;
            rangeRange[tableId].ed = upperIndex;
            // std::cout << "(" << lowerIndex << ", " << upperIndex << ")  " << std::endl;
        }
        // std::cout << std::endl;

        if (valid)
        {
            RangeVecIterator iter(rangeRange);
            while (iter)
            {
                auto rangeVec = iter.Get();
                auto tuple = nextRangeTable.AcquireTuple();

                {
                    size_t st = 0;
                    size_t ed = joinRelation.Length();

                    for (size_t attrSeq = 0; attrSeq < attrSortSeq.size(); attrSeq++)
                    {
                        auto& attr = attrSortSeq[attrSeq];
                        auto& attrData = joinRelation[attr].get();
                        int expectedValue = currentValue[attrSeq];
                        st = std::lower_bound(attrData.Raw().begin(), attrData.Raw().end(), expectedValue) - attrData.Raw().begin();
                        ed = std::upper_bound(attrData.Raw().begin(), attrData.Raw().end(), expectedValue) - attrData.Raw().begin();
                    }

                    tuple[ehPlan->mRelationId] = {st, ed};
                }

                for (size_t tableIndex = 0; tableIndex < tableRefs.size(); tableIndex++)
                {
                    auto& table = tableRefs[tableIndex].get();
                    RangeTuple rangetuple = table[sortIndicesVec[tableIndex][rangeVec[tableIndex]]];
                    for (auto relId : table.GetRelIndices())
                        tuple[relId] = rangetuple[relId];
                }
                iter++;
            }
        }

        preValue = currentValue;
    }

    return nextRangeTable;
}

RangeTable GenericJoin::Execute(std::unique_ptr<LTPlan> plan)
{
    if (plan->IsEH())
        return ExecuteEH(std::move(plan));
    else if (plan->SubPlanNum() <= 1)
        return ExecuteSingle(std::move(plan));
    else if (plan->SubPlanNum() > 1)
        return ExecuteMuti(std::move(plan));
}


Relation GenericJoin::operator()()
{
    auto rangeTable = Execute(std::move(mPlan));

    std::cout << "Join results number: " << rangeTable.Length() << std::endl;
    // rangeTable.Print();

    // Convert range table to relation
    Relation result;
    /*
    for (size_t attrIndex = 0; attrIndex < rangeTable.Width(); attrIndex++)
    {
        std::string attrName = mAttrs[attrIndex];
        std::vector<int> attrData(rangeTable.Length());

        for (auto& rel : mRelations)
            if (rel.ExistAttr(attrName))
            {
                const auto& attr = rel[attrName];
                for (size_t tupleIndex = 0; tupleIndex < rangeTable.Length(); tupleIndex++)
                {
                    RangeTuple rangeTuple = rangeTable[tupleIndex];
                    int value = attr[rangeTuple[attrIndex].st];
                    attrData[tupleIndex] = value;
                }

                break;
            }

        result.SetTupleNum(attrData.size());
        result.Insert(attrName, std::move(attrData));
    }

    std::cout << "Final result: " << result.Length() <<std::endl;
    */

    return result;

    /*
    // create the primary range table
    RangeTable rangeTable(mRelations.size(), 1);
    {
        RangeTuple tuple = rangeTable.AcquireTuple();
        for (int i = 0; i < mRelations.size(); i++)
        {
            rangeTable[0][i].st = 0;
            rangeTable[0][i].ed = mRelations[i].Length();
        }
    }

    // for (const auto& attr : mPlan)
    for (size_t attrIndexx = 0; attrIndexx < mPlan.size(); attrIndexx++)
    {
        const auto& attr = mPlan[attrIndexx];

        if constexpr (Debug)
        {
            std::cout << std::endl << "Joining attribute " << attr << std::endl;
            std::cout << "There exists " << rangeTable.Length() << " tuples in range table." << std::endl;
        }

        if (rangeTable.Length() == 0)
        {
            if constexpr(Debug)
                std::cout << "No range tuples left, stopping join." << std::endl;
            break;
        }

        auto relationIndices  = SelectRelationIndices(attr, true);
        auto relationIndicesC = SelectRelationIndices(attr, false);

        auto attrsData = FetchAttributes(relationIndices, attr);

        RangeTable nextRangeTable = CreateEstimatedRangeTable(rangeTable, relationIndices, mRelations.size());

        for (size_t rangeTupleIndex = 0; rangeTupleIndex < rangeTable.Length(); rangeTupleIndex++)
        {
            RangeTuple rangeTuple = rangeTable[rangeTupleIndex];
            size_t shortestRelationIndex = ShortestRangeIndex(rangeTuple, relationIndices);

            {
                Range baseRange = rangeTuple[shortestRelationIndex];
                auto baseAttr = mRelations[shortestRelationIndex][attr];

                for (size_t attrIndex = baseRange.st; attrIndex < baseRange.ed; attrIndex++)
                {
                    int value = baseAttr.get()[attrIndex];
                    if (attrIndex != baseRange.st and value == baseAttr.get()[attrIndex-1])
                        continue;
                    
                    bool valueExsit = true;
                    {
                        for (size_t relationIndex : relationIndices)
                        {
                            auto targetAttr = mRelations[relationIndex][attr];
                            size_t startQueryIndex = rangeTuple[relationIndex].st;
                            size_t endQueryIndex   = rangeTuple[relationIndex].ed;

                            const auto& [start, end] = targetAttr.get().Query(startQueryIndex, endQueryIndex, value);
                            if (start == end)
                            {
                                valueExsit = false;
                                break;
                            }
                        }
                    }

                    if (valueExsit)
                    {
                        RangeTuple storeTuple = nextRangeTable.AcquireTuple();

                        for (size_t relationIndex : relationIndices)
                        {
                            auto targetAttr = mRelations[relationIndex][attr];
                            size_t startQueryIndex = rangeTuple[relationIndex].st;
                            size_t endQueryIndex   = rangeTuple[relationIndex].ed;

                            const auto& [start, end] = targetAttr.get().Query(startQueryIndex, endQueryIndex, value);
                            storeTuple[relationIndex].st = start - targetAttr.get().Begin();
                            storeTuple[relationIndex].ed = end - targetAttr.get().Begin();
                        }

                        for (size_t relationIndex : relationIndicesC)
                        {
                            storeTuple[relationIndex].st = rangeTuple[relationIndex].st;
                            storeTuple[relationIndex].ed = rangeTuple[relationIndex].ed;
                        }
                    }
                }
            }
        }

        std::swap(nextRangeTable, rangeTable);
        std::cout << "Attribute " << attr << " is joined" << std::endl;
    }

    // Convert range table to relation
    Relation result;
    for (size_t attrIndex = 0; attrIndex < mPlan.size(); attrIndex++)
    {
        std::string attrName = mPlan[attrIndex];
        std::vector<int> attrData(rangeTable.Length());

        for (auto& rel : mRelations)
            if (rel.ExistAttr(attrName))
            {
                auto& attr = rel[attrName].get();
                for (size_t tupleIndex = 0; tupleIndex < rangeTable.Length(); tupleIndex++)
                {
                    RangeTuple rangeTuple = rangeTable[tupleIndex];
                    int value = attr[rangeTuple[attrIndex].st];
                    attrData[tupleIndex] = value;
                }

                break;
            }

        result.SetTupleNum(attrData.size());
        result.Insert(attrName, std::move(attrData));
    }

    std::cout << "Final result: " << result.Length() <<std::endl;

    return result;
    */
}


RangeTable GenericJoin::SingleAttrWCOJoin(RangeTableRef rangeTableRef, std::vector<size_t>& relIndices, std::string attr, double cost)
{
    auto& rangeTable = rangeTableRef.get();
    std::vector<size_t> relationIndices;//SelectRelationIndices(attr, true);
    for (size_t i : relIndices)
        if (mRelations[i].ExistAttr(attr))
            relationIndices.push_back(i);
    std::set<size_t> relationIndicesCSet;
    for (size_t i = 0; i < mRelations.size(); i++)
        relationIndicesCSet.insert(i);
    for (size_t i : relationIndices)
        relationIndicesCSet.erase(i);
    std::vector<size_t> relationIndicesC{relationIndicesCSet.begin(), relationIndicesCSet.end()};


    // auto attrsData = FetchAttributes(relIndices, attr);

    RangeTable nextRangeTable = CreateEstimatedRangeTable(rangeTable, relationIndices, mRelations.size(), cost);
    if (rangeTableRef.get().GetRelIndices().size() == 0)
    {
        nextRangeTable.GetRelIndices() = std::set<size_t>{relIndices.begin(), relIndices.end()};
    }
    else 
    {
        nextRangeTable.GetRelIndices() = std::move(rangeTableRef.get().GetRelIndices());
        for (size_t i : relIndices)
            nextRangeTable.AddRelName(i);
    }

    // for (size_t index : relationIndices)
    //     nextRangeTable.AddRelName(index);

    for (size_t rangeTupleIndex = 0; rangeTupleIndex < rangeTable.Length(); rangeTupleIndex++)
    {
        RangeTuple rangeTuple = rangeTable[rangeTupleIndex];
        size_t shortestRelationIndex = ShortestRangeIndex(rangeTuple, relationIndices);

        {
            Range baseRange = rangeTuple[shortestRelationIndex];
            auto& baseAttr = mRelations[shortestRelationIndex][attr].get();

            for (size_t attrIndex = baseRange.st; attrIndex < baseRange.ed; attrIndex++)
            {
                int value = baseAttr[attrIndex];
                if (attrIndex != baseRange.st and value == baseAttr[attrIndex-1])
                    continue;
                
                bool valueExsit = true;
                {
                    for (size_t relationIndex : relationIndices)
                    {
                        auto& targetAttr = mRelations[relationIndex][attr].get();
                        size_t startQueryIndex = rangeTuple[relationIndex].st;
                        size_t endQueryIndex   = rangeTuple[relationIndex].ed;

                        const auto& [start, end] = targetAttr.Query(startQueryIndex, endQueryIndex, value);
                        if (start == end)
                        {
                            valueExsit = false;
                            break;
                        }
                    }
                }

                if (valueExsit)
                {
                    RangeTuple storeTuple = nextRangeTable.AcquireTuple();

                    for (size_t relationIndex : relationIndices)
                    {
                        auto& targetAttr = mRelations[relationIndex][attr].get();
                        size_t startQueryIndex = rangeTuple[relationIndex].st;
                        size_t endQueryIndex   = rangeTuple[relationIndex].ed;

                        const auto& [start, end] = targetAttr.Query(startQueryIndex, endQueryIndex, value);
                        storeTuple[relationIndex].st = start - targetAttr.Begin();
                        storeTuple[relationIndex].ed = end - targetAttr.Begin();
                    }

                    for (size_t relationIndex : relationIndicesC)
                    {
                        storeTuple[relationIndex].st = rangeTuple[relationIndex].st;
                        storeTuple[relationIndex].ed = rangeTuple[relationIndex].ed;
                    }
                }
            }
        }
    }

    if constexpr (Debug)
    {
        std::cout << "WCO join on " << attr << " result size: " << nextRangeTable.Length() << std::endl;
    }

    return nextRangeTable;
}

RangeTable GenericJoin::SingleAttrCartesianJoin(std::vector<RangeTableRef>& tableRefs, double cost)
{
    // std::cout << "Table size: ";
    // for (auto& table : tableRefs) std::cout << table.get().Length() << std::endl;
    // std::cout << std::endl;
    RangeTable nextRangeTable = CreateFullEstimatedRangeTable(tableRefs, mRelations.size(), cost);
    RangeTableIterator iter = CreateRangeTableIter(tableRefs);

    while (iter)
    {
        auto& rangeTupleIndices = iter.Get();

        auto rtuple = nextRangeTable.AcquireTuple();
        for (size_t tableId = 0; tableId < tableRefs.size(); tableId++)
        {
            auto& table = tableRefs[tableId].get();
            for (size_t relId : table.GetRelIndices())
            {
                rtuple[relId] = table[rangeTupleIndices[tableId]][relId];
            }
        }

        iter++;
    }

    return nextRangeTable;
}


// Loop is not loop~
RangeTable GenericJoin::SingleAttrLoopJoin(std::vector<RangeTableRef>& tableRefs, std::vector<size_t>& relIndices, std::string attr, double cost)
{
    std::vector<size_t> relationIndices;//SelectRelationIndices(attr, true);
    for (size_t i : relIndices)
        if (mRelations[i].ExistAttr(attr))
            relationIndices.push_back(i);

    // std::cout << "Looped relIndex: ";
    // for (size_t i : relationIndices)
    //     std::cout << i << ' ';
    // std::cout << std::endl;

    std::set<size_t> relationIndicesCSet;
    for (size_t i = 0; i < mRelations.size(); i++)
        relationIndicesCSet.insert(i);
    for (size_t i : relationIndices)
        relationIndicesCSet.erase(i);
    std::vector<size_t> relationIndicesC{relationIndicesCSet.begin(), relationIndicesCSet.end()};
    // auto relationIndicesC = SelectRelationIndices(attr, false);

    // tracked relation index for each range table
    std::vector<size_t> trackedRelIndices(tableRefs.size());
    for (size_t tableIndex = 0; tableIndex < tableRefs.size(); tableIndex++)
    {
        auto& table = tableRefs[tableIndex].get();
        // std::cout << "  rels in range table: ";
        // for (auto& rel : table.GetRelIndices())
        //     std::cout << rel << ' ';
        // std::cout << std::endl;
        for (size_t relIndex : relationIndices)
        {
            if ( table.ExistRel(relIndex) )
            {
                trackedRelIndices[tableIndex] = relIndex;
                // std::cout << "tracked: " << tableIndex << " " << relIndex << std::endl;
                break;
            }
        }
    }

    std::vector<AttributeRef<int>> trackedAttrData;
    for (size_t tableIndex = 0; tableIndex < tableRefs.size(); tableIndex++)
    {
        auto& table = tableRefs[tableIndex].get();
        trackedAttrData.emplace_back(mRelations[trackedRelIndices[tableIndex]][attr]);
    }

    // sort every range table
    std::vector<std::vector<size_t>> sortIndices;
    // std::vector<std::vector<int>> sortAttrValueVec;
    Timer tim("sort");
    for (size_t tableId = 0; tableId < tableRefs.size(); tableId++)
    {
        size_t trackedRelId = trackedRelIndices[tableId];
        auto& table = tableRefs[tableId].get();
        sortIndices.emplace_back(table.LazySort(mRelations, trackedRelId, attr));

        // std::vector<int> sortAttrValue(table.Length());
        // std::transform(sortIndices.back().begin(), sortIndices.back().end(), sortAttrValue.begin(),
        //     [&](size_t sid){
        //         RangeTuple rtuple = table[sid];
        //         size_t st = rtuple[trackedRelId].st;
        //         return mRelations[trackedRelId][attr].get()[st];
        //     }
        // );

        // sortAttrValueVec.push_back(sortAttrValue);
    }
    // std::cout << "sorting time: " << tim.Timing() << std::endl;


    // find the shortest range table
    int shortestRTIndex = 0;
    for (int i = 0; i < sortIndices.size(); i++)
    {
        if ( sortIndices[i].size() < sortIndices[shortestRTIndex].size() )
            shortestRTIndex = i;
    }

    auto& shortestSortInd = sortIndices[shortestRTIndex];
    auto& shortestRangeTable = tableRefs[shortestRTIndex].get();
    auto& shortestAttrData = trackedAttrData[shortestRTIndex].get();

    RangeTable nextRangeTable = CreateFullEstimatedRangeTable(tableRefs, mRelations.size(), cost);
    std::vector<Range> rangeRange(tableRefs.size());

    int preValue = -1;
    for (size_t seq = 0; seq < shortestSortInd.size(); seq++)
    {
        // auto expectedValue = sortAttrValueVec[shortestRTIndex][seq]; // Rt2Value(seq2Rt(shortestRTIndex, seq), trackedRelIndices[shortestRTIndex]);
        RangeTuple expectedTuple = shortestRangeTable[shortestSortInd[seq]];
        size_t expectedOffset = expectedTuple[trackedRelIndices[shortestRTIndex]].st;
        int expectedValue = shortestAttrData[expectedOffset];
        if (expectedValue == preValue)
            continue;

        // std::cout << "  expected value: " << expectedValue << std::endl;

        // check if expected value appears in all every range table
        bool valid = true;
        for (size_t tableIndex = 0; tableIndex < tableRefs.size(); tableIndex++)
        {
            auto& table = tableRefs[tableIndex].get();
            size_t trackedRelId = trackedRelIndices[tableIndex];
            auto& tableAttrData = trackedAttrData[tableIndex].get();

            auto findLow = [&](size_t off, int targetValue){
                size_t dataoff = table[off][trackedRelId].st;
                int storeValue = tableAttrData[dataoff];
                return storeValue < targetValue;
            };
            auto findUp = [&](int targetValue, size_t off){
                size_t dataoff = table[off][trackedRelId].st;
                int storeValue = tableAttrData[dataoff];
                return storeValue > targetValue;
            };

            auto lowerIndex = std::lower_bound(sortIndices[tableIndex].begin(), sortIndices[tableIndex].end(), expectedValue, findLow) - sortIndices[tableIndex].begin();
            auto upperIndex = std::upper_bound(sortIndices[tableIndex].begin(), sortIndices[tableIndex].end(), expectedValue, findUp) - sortIndices[tableIndex].begin();

            if (lowerIndex >= upperIndex)
            {
                valid = false;
                break;
            }

            // record range tuples' ranges
            rangeRange[tableIndex].st = lowerIndex;
            rangeRange[tableIndex].ed = upperIndex;
            // std::cout << "   range: " << lowerIndex << " - " << upperIndex << std::endl;
        }

        // merge
        if (valid)
        {
            RangeVecIterator iter(rangeRange);
            while (iter)
            {
                auto rangeVec = iter.Get();
                auto tuple = nextRangeTable.AcquireTuple();
                for (size_t tableIndex = 0; tableIndex < tableRefs.size(); tableIndex++)
                {
                    auto& table = tableRefs[tableIndex].get();
                    RangeTuple rangetuple = table[sortIndices[tableIndex][rangeVec[tableIndex]]];
                    for (auto relId : table.GetRelIndices())
                        tuple[relId] = rangetuple[relId];
                }
                iter++;
            }
        }

        preValue = expectedValue;
    }

    // add joined relations
    // for (auto& tableRef : tableRefs)
    //     nextRangeTable.GetRelIndices().insert(tableRef.get().GetRelIndices().begin(), tableRef.get().GetRelIndices().end());

    // RangeTableIterator iter = CreateRangeTableIter(tableRefs);

    /*
    while (iter)
    {
        auto& rangeTupleIndices = iter.Get();
        RangeTuple firstRangeTuple = tableRefs[0].get()[rangeTupleIndices[0]];

        for (size_t i = 0; i < ranges.size(); i++)
        {
            ranges[i].st = firstRangeTuple[i].st;
            ranges[i].ed = firstRangeTuple[i].ed;
        }

        // check valid
        bool invalid = false;
        for (size_t rangeTableIndex = 1; rangeTableIndex < tableRefs.size(); rangeTableIndex++)
        {
            auto& rangeTable = tableRefs[rangeTableIndex].get();
            RangeTuple rangeTuple = rangeTable[rangeTupleIndices[rangeTableIndex]];

            for (auto relationIndex : relationIndices)
            {
                if (ranges[relationIndex].st < rangeTuple[relationIndex].st)
                    rangeTuple[relationIndex].st = ranges[relationIndex].st;
                if (ranges[relationIndex].ed > rangeTuple[relationIndex].ed)
                    ranges[relationIndex].ed = rangeTuple[relationIndex].ed;

                if (ranges[relationIndex].st >= ranges[relationIndex].ed)
                {
                    invalid = true;
                    break;
                }
            }

            if (invalid)
                break;
        }

        // merge
        if (!invalid)
        {
            for (size_t i = 0; i < tableRefs.size(); i++)
            {
                auto& rangeTable = tableRefs[i].get();
                RangeTuple rangeTuple = rangeTable[rangeTupleIndices[i]];

                for (size_t j = 0; j < rangeTable.Width(); j++)
                {
                    size_t relationIndex = j;
                    if (ranges[relationIndex].st < rangeTuple[relationIndex].st)
                        rangeTuple[relationIndex].st = ranges[relationIndex].st;
                    if (ranges[relationIndex].ed > rangeTuple[relationIndex].ed)
                        ranges[relationIndex].ed = rangeTuple[relationIndex].ed;
                }
            }

            auto tuple = nextRangeTable.AcquireTuple();
            for (size_t i = 0; i < ranges.size(); i++)
            {
                tuple[i].st = ranges[i].st;
                tuple[i].ed = ranges[i].ed;
            }
        }
    }
    */

    return nextRangeTable;
}

