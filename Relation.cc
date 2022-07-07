#include "Relation.h"

#include <algorithm>
#include <iostream>
#include <numeric>

Relation::Relation()
    : mTupleNum(0), mName("")
{}

void Relation::Insert(const std::string& attr, std::vector<int>&& data)
{
    mAttributes.emplace(attr, std::move(data));
}


void Relation::Sort(std::vector<std::string>& attrOrder)
{
    std::vector<AttributeRef<int>> attrbuteRefs;
    for (auto& attr : attrOrder)
    {
        if (ExistAttr(attr))
        {
            attrbuteRefs.push_back(operator[](attr));
        }
    }

    auto cmp = [&](size_t index1, size_t index2){
        size_t i;
        for (i = 0; i < attrbuteRefs.size(); i++)
        {
            auto& attr = attrbuteRefs[i];
            if (attr.get()[index1] != attr.get()[index2])
                return attrbuteRefs[i].get()[index1] < attrbuteRefs[i].get()[index2];
        }
        return false;
    };

    std::vector<size_t> indices(Length());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), cmp);

    for (size_t i = 0; i < attrbuteRefs.size(); i++)
    {
        std::vector<int> data(Length());
        for (size_t j = 0; j < Length(); j++)
            data[j] = attrbuteRefs[i].get()[indices[j]];
        
        attrbuteRefs[i].get() = std::move(data);
    }
}




