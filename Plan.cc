#include "Plan.h"
#include "Relation.h"
#include <algorithm>


std::vector<Relation> GloablData::GRelation;
std::vector<std::string> GloablData::GAttributes;

namespace
{

std::unique_ptr<RangeTable> CreatePrimaryRangeTable()
{
    std::unique_ptr<RangeTable> table = std::make_unique<RangeTable>(GloablData::GRelation.size(), 1);
    return table;
}

std::unique_ptr<RangeTable> CreateRangeTable(size_t tupleNum)
{
    tupleNum = std::min(tupleNum, (size_t)5e7);
    std::unique_ptr<RangeTable> table = std::make_unique<RangeTable>(GloablData::GRelation.size(), tupleNum);
    return table;
}

std::vector<AttributeRef<int>> ExtractAttrDataFromRelations(std::vector<size_t>& relIdVec, std::string attr)
{
    std::vector<AttributeRef<int>> attrData;
    for (auto relId : relIdVec)
        attrData.emplace_back(GloablData::GRelation[relId][attr]);
    return attrData;
}

size_t ShortestRelation(std::vector<size_t>& relIdVec)
{
    size_t shortRelId = 0;
    for (auto relId : relIdVec)
        if (GloablData::GRelation[relId].Length() < GloablData::GRelation[shortRelId].Length())
            shortRelId = relId;
    return shortRelId;
}

size_t RangeTableExpandLength(RangeTable* table, size_t relId)
{
    size_t length = 0;
    for (size_t rtupleId = 0; rtupleId < table->Length(); rtupleId++)
        length += table[rtupleId][relId]->Length();
    return length;
}

size_t ShortestRelIdInRangeTable(RangeTable* table, std::vector<size_t>& relIdVec)
{
    std::vector<size_t> expandLengths(relIdVec.size());
    for (size_t relRelId = 0; relRelId < relIdVec.size(); relRelId++)
        expandLengths[relRelId] = RangeTableExpandLength(table, relIdVec[relRelId]);

    size_t shortRelRelId = std::min(expandLengths.begin(), expandLengths.end()) - expandLengths.begin();
    return relIdVec[shortRelRelId];
}

size_t ShortestTable(std::vector<std::unique_ptr<RangeTable>>& tables)
{
    size_t shortTableId = 0;
    for (size_t tableId = 0; tableId < tables.size(); tableId++)
        if (tables[shortTableId]->Length() > tables[tableId]->Length())
            shortTableId = tableId;
    return shortTableId;
}

size_t SelectRelIdFromTableWithAttr(RangeTable* table, std::string attr)
{
    for (size_t relId : table->GetRelIndices())
        if (GloablData::GRelation[relId].ExistAttr(attr))
            return relId;
}


std::vector<size_t> SortRangeTable(RangeTable* table, std::vector<size_t>& relIdVec, std::vector<std::string>& attrVec)
{
    std::vector<size_t> indices(table->Length());
    std::iota(indices.begin(), indices.end(), 0);

    // sorted attrdata
    std::vector<AttributeRef<int>> attrDataVec;
    for (size_t relRelId = 0; relRelId < relIdVec.size(); relRelId++)
        attrDataVec.emplace_back(GloablData::GRelation[relIdVec[relRelId]][attrVec[relRelId]]);
    
    std::sort(indices.begin(), indices.end(),
        [&](size_t rtupleId1, size_t rtupleId2){
            RangeTuple rtuple1 = table->operator[](rtupleId1);
            RangeTuple rtuple2 = table->operator[](rtupleId2);

            for (size_t relRelId = 0; relRelId < relIdVec.size(); relRelId++)
            {
                size_t relId = relIdVec[relRelId];
                size_t valueIndex1 = rtuple1[relId].st;
                size_t valueIndex2 = rtuple2[relId].st;
                int value1 = attrDataVec[relRelId].get()[valueIndex1];
                int value2 = attrDataVec[relRelId].get()[valueIndex2];

                if (value1 != value2)
                    return value1 < value2;
            }

            return false;
        }
    );

    return indices;
}

} // namespace




std::unique_ptr<RangeTable> SingleAttrPlan::Execute()
{
    if (SubPlans.size() == 0)
    {
        std::cout << "Leaf" << std::endl;
        return ExeLeaf();
    }
    else if (SubPlans.size() == 1)
    {
        std::cout << "Node" << std::endl;
        return ExeNode();
    }
    else
    {
        std::cout << "Multi" << std::endl;
        return ExeMulti();
    }
}

std::unique_ptr<RangeTable> SingleAttrPlan::ExeLeaf()
{
    std::vector<AttributeRef<int>> targetAttrDataVec = ExtractAttrDataFromRelations(RelIdWithAttr, TargetAttr);

    // Select shortest relation
    size_t shortRelId = ShortestRelation(RelIdWithAttr);
    AttributeRef<int> shortAttrData = GloablData::GRelation[shortRelId][TargetAttr];

    std::cout << "leaf allocate size: " << GloablData::GRelation[shortRelId].Length() << std::endl;
    std::unique_ptr<RangeTable> resultTable = CreateRangeTable(100 + GloablData::GRelation[shortRelId].Length());
    // Insert related relations
    for (size_t relId : RelIdWithAttr)
        resultTable->GetRelIndices().insert(relId);

    size_t allcSize = 0;
    std::vector<Range> valueRange(RelIdWithAttr.size());
    for (int value : shortAttrData.get().Raw())
    {
        bool ExistValue = true;
        for (size_t relRelId = 0; relRelId < RelIdWithAttr.size(); relRelId++)
        {
            auto& attrData = targetAttrDataVec[relRelId].get().Raw();
            size_t st = std::lower_bound(attrData.begin(), attrData.end(), value) - attrData.begin();
            size_t ed = std::upper_bound(attrData.begin(), attrData.end(), value) - attrData.begin();

            if (st >= ed)
            {
                ExistValue = false;
                break;
            }
            valueRange[relRelId].st = st;
            valueRange[relRelId].ed = ed;
        }

        if (!ExistValue)
            continue;
        allcSize++;
        RangeTuple rtuple = resultTable->AcquireTuple();
        for (size_t relId = 0; relId < GloablData::GRelation.size(); relId++)
        {
            rtuple[relId].st = 0;
            rtuple[relId].ed = GloablData::GRelation[relId].Length();
        }
        for (size_t relRelId = 0; relRelId < RelIdWithAttr.size(); relRelId++)
        {
            size_t relId = RelIdWithAttr[relRelId];
            rtuple[relId] = valueRange[relRelId];
        }
    }
    std::cout << "Used size: " << allcSize << std::endl;

    return resultTable;
}



std::unique_ptr<RangeTable> SingleAttrPlan::ExeNode()
{
    std::unique_ptr<RangeTable> subRangeTable = SubPlans[0]->Execute();
    
    std::cout << "node allocate size: " << EstimateLength << std::endl;
    std::unique_ptr<RangeTable> resultTable = CreateRangeTable((size_t)(EstimateLength+ 1000));
    // merge calculated relations
    resultTable->GetRelIndices().insert(subRangeTable->GetRelIndices().begin(), subRangeTable->GetRelIndices().end());
    resultTable->GetRelIndices().insert(RelIdWithAttr.begin(), RelIdWithAttr.end());

    // size_t shortestRelId = ShortestRelIdInRangeTable(subRangeTable.get(), RelIdWithAttr);
    std::vector<AttributeRef<int>> targetAttrData;
    for (size_t relId : RelIdWithAttr)
        targetAttrData.emplace_back(GloablData::GRelation[relId][TargetAttr]);

    size_t allocSize =0;
    std::vector<size_t> valueRange;
    for (size_t rtupleId = 0; rtupleId < subRangeTable->Length(); rtupleId++)
    {
        RangeTuple rtuple = subRangeTable->operator[](rtupleId);
        size_t shortestRelId = ShortestRelIdInRangeTuple(rtuple, RelIdWithAttr);
        size_t shortStart = rtuple[shortestRelId].st;
        size_t shortEnd   = rtuple[shortestRelId].ed;

        auto& shortestAttrData = GloablData::GRelation[shortestRelId][TargetAttr].get();

        std::vector<Range> valueRange(GloablData::GRelation.size());
        for (size_t offset = shortStart; offset < shortEnd; offset++)
        {
            int value = shortestAttrData[offset];
            // check if value appears before
            if (offset != shortStart and shortestAttrData[offset-1]==value)
                continue;

            bool ExistValue = true;
            for (size_t relRelId = 0; relRelId < RelIdWithAttr.size(); relRelId++)
            {
                auto& attrData = targetAttrData[relRelId].get().Raw();
                size_t st = std::lower_bound(attrData.begin(), attrData.end(), value) - attrData.begin();
                size_t ed = std::upper_bound(attrData.begin(), attrData.end(), value) - attrData.begin();

                if (st >= ed)
                {
                    ExistValue = false;
                    break;
                }

                valueRange[relRelId].st = st;
                valueRange[relRelId].ed = ed;
            }

            if (!ExistValue)
                continue;

            allocSize++;
            RangeTuple newtuple = resultTable->AcquireTuple();
            for (size_t relId = 0; relId < GloablData::GRelation.size(); relId++)
                newtuple[relId] = rtuple[relId];
            for (size_t relRelId = 0; relRelId < RelIdWithAttr.size(); relRelId++)
            {
                size_t relId = RelIdWithAttr[relRelId];
                newtuple[relId] = valueRange[relRelId];
            }
        }
    }
    std::cout << "Used Size: " << allocSize << std::endl;

    return resultTable;
}



std::unique_ptr<RangeTable> SingleAttrPlan::ExeMulti()
{
    std::vector<std::unique_ptr<RangeTable>> subRangeTables;
    for (auto& subplan : SubPlans)
        subRangeTables.emplace_back(subplan->Execute());
    

    std::unique_ptr<RangeTable> resultTable = CreateRangeTable((size_t)(EstimateLength+ 10));
    // merge calculated relations
    for (auto& subtable : subRangeTables)
        resultTable->GetRelIndices().insert(subtable->GetRelIndices().begin(), subtable->GetRelIndices().end());
    resultTable->GetRelIndices().insert(RelIdWithAttr.begin(), RelIdWithAttr.end());

    // select the shortest expand relId in each sub table
    std::vector<size_t> trackedRelIdVec;
    for (auto& subtable : subRangeTables)
    {
        for (size_t relId : subtable->GetRelIndices())
            if (GloablData::GRelation[relId].ExistAttr(TargetAttr))
            {
                trackedRelIdVec.emplace_back(relId);
                break;
            }
    }

    // tracked attribute data
    std::vector<AttributeRef<int>> trackedAttrData;
    for (size_t relId : trackedRelIdVec)
        trackedAttrData.emplace_back(GloablData::GRelation[relId][TargetAttr]);

    // get the sort indices
    std::vector<std::vector<size_t>> sortIndicesVec;
    for (size_t tableId = 0; tableId < subRangeTables.size(); tableId++)
    {
        std::vector<size_t> sortRelId = {trackedRelIdVec[tableId]};
        std::vector<std::string> sortAttr = {TargetAttr};
        sortIndicesVec.emplace_back(SortRangeTable(subRangeTables[tableId].get(), sortRelId, sortAttr));
    }

    size_t shortTableId = ShortestTable(subRangeTables);
    size_t shortRelId = trackedRelIdVec[shortTableId];
    auto& shortAttrData = GloablData::GRelation[shortRelId][TargetAttr].get().Raw();

    // join
    std::vector<Range> valueRange(subRangeTables.size());
    for (size_t shorttupleId = 0; shorttupleId < subRangeTables[shortTableId]->Length(); shorttupleId++)
    {
        RangeTuple shortTuple = subRangeTables[shortTableId].get()->operator[](shorttupleId);
        int targetValue = shortAttrData[shortTuple[shortRelId].st];

        bool ExistValue = true;
        for (size_t tableId = 0; tableId < subRangeTables.size(); tableId++)
        {
            RangeTable* subTable = subRangeTables[tableId].get();

            auto findLow = [&](size_t element, int targetValue){
                RangeTuple tuple = subRangeTables[tableId]->operator[](element);
                size_t st = tuple[trackedRelIdVec[tableId]].st;
                int storeValue = trackedAttrData[tableId].get()[st];
                return storeValue < targetValue;
            };
            auto findUp  = [&](int targetValue, size_t element){
                RangeTuple tuple = subRangeTables[tableId]->operator[](element);
                size_t st = tuple[trackedRelIdVec[tableId]].st;
                int storeValue = trackedAttrData[tableId].get()[st];
                return storeValue > targetValue;
            };
            
            size_t st = std::lower_bound(sortIndicesVec[tableId].begin(), sortIndicesVec[tableId].end(), targetValue, findLow) - sortIndicesVec[tableId].begin();
            size_t ed = std::upper_bound(sortIndicesVec[tableId].begin(), sortIndicesVec[tableId].end(), targetValue, findUp) - sortIndicesVec[tableId].begin();

            if (st >= ed)
            {
                ExistValue = false;
                break;
            }

            valueRange[tableId] = {st, ed};
        }

        if (!ExistValue)
            continue;
        
        // merge for every pair of range tuple in each sub range table
        {
            RangeVecIterator iter(valueRange);
            while(iter)
            {
                auto& rtupleIdIndices = iter.Get();
                auto newtuple = resultTable->AcquireTuple();

                for (size_t relId = 0; relId < GloablData::GRelation.size(); relId++)
                {
                    auto firstTableTuple = subRangeTables[0]->operator[](rtupleIdIndices[0]);
                    newtuple[relId] = firstTableTuple[relId];
                }

                for (size_t tableId = 1; tableId < subRangeTables.size(); tableId++)
                {
                    for (size_t relId : subRangeTables[tableId]->GetRelIndices())
                    {
                        auto tableTuple = subRangeTables[tableId]->operator[](rtupleIdIndices[tableId]);
                        newtuple[relId] = tableTuple[relId];
                    }
                }

                iter++;
            }
        }
    }

    return resultTable;
}

std::unique_ptr<RangeTable> CartesianPlan::Execute()
{
    std::vector<std::unique_ptr<RangeTable>> subRangeTables;
    for (auto& subplan : SubPlans)
    {
        subRangeTables.emplace_back(subplan->Execute());
        std::cout << "result length: " << subRangeTables.back()->Length() << std::endl;
    }

    
    size_t resultLength = 1;
    for (auto& subtable : subRangeTables)
        resultLength *= subtable->Length();
    std::unique_ptr<RangeTable> resultTable = CreateRangeTable(resultLength + 10);
    // merge calculated relations
    for (auto& subtable : subRangeTables)
        resultTable->GetRelIndices().insert(subtable->GetRelIndices().begin(), subtable->GetRelIndices().end());

    // merge result
    std::vector<size_t> subTableLengthVec(subRangeTables.size());
    for (size_t tableId = 0; tableId < subRangeTables.size(); tableId++)
        subTableLengthVec[tableId] = subRangeTables[tableId]->Length();
    RangeTableIterator iter(subTableLengthVec);
    while(iter)
    {
        auto& rtupleIndices = iter.Get();

        RangeTuple newTuple = resultTable->AcquireTuple();

        // copy the tuple from first sub table
        auto firstTable = subRangeTables[0].get();
        for(size_t relId = 0; relId < GloablData::GRelation.size(); relId++)
        {
            newTuple[relId].st = firstTable[rtupleIndices[0]][relId]->st;
            newTuple[relId].ed = firstTable[rtupleIndices[0]][relId]->ed;
        }

        for (size_t tableId = 1; tableId < subRangeTables.size(); tableId++)
        {
            auto table = subRangeTables[tableId].get();
            for (size_t relId : table->GetRelIndices())
                newTuple[relId] = table->operator[](rtupleIndices[tableId])[relId];
        }

        iter++;
    }

    return resultTable;
}
