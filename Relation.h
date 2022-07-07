#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>


template<typename T>
class Attribute
{
public:
    Attribute(std::vector<T>&& data)
        : mData(std::forward<std::vector<T>&&>(data))
    {
    }

    Attribute<T>& operator=(std::vector<T>&& data)
    {
        std::swap(mData, data);

        return *this;
    }

    auto Query(size_t startIndex, size_t endIndex, T value) const
    {
        auto start = mData.begin() + startIndex;
        auto end   = mData.begin() + endIndex;

        auto lowerIter  = std::lower_bound(start, end, value);
        auto upperIter = std::upper_bound(start, end, value);

        return std::make_pair(lowerIter, upperIter);
    }

    auto Begin() const
    {
        return mData.begin();
    }

    T operator[](unsigned int index) const { return mData[index]; }

    std::vector<T>& Raw() { return mData; }

private:
    std::vector<T> mData;
};

template<typename T>
using AttributeRef = std::reference_wrapper<Attribute<T>>;

class Relation
{
public:
    Relation();

    Relation(Relation&& relation)
        : mName(relation.mName), mTupleNum(relation.mTupleNum)
    {
        mAttributes.insert(std::make_move_iterator(relation.mAttributes.begin()),
                           std::make_move_iterator(relation.mAttributes.end()));
        relation.mAttributes.clear();
    }

    Relation& operator=(Relation&& relation)
    {
        mName = relation.mName;
        mTupleNum = relation.mTupleNum;
        if (mAttributes.size())
            mAttributes.clear();
        mAttributes.insert(std::make_move_iterator(relation.mAttributes.begin()),
                           std::make_move_iterator(relation.mAttributes.end()));
        relation.mAttributes.clear();
        return *this;
    }

    void Insert(const std::string& attr, std::vector<int>&& data);

    bool ExistAttr(std::string_view attr) const
    {
        return mAttributes.find(attr) != mAttributes.end();
    }

    size_t Length() const { return mTupleNum; }
    const auto& Attrs() const { return mAttributes; }

    AttributeRef<int> operator[](std::string_view attr)
    {
        return mAttributes.find(attr)->second;
    }

    const std::string& Name() const
    {
        return mName;
    }

    void Filter();

    void Sort(std::vector<std::string>& attrOrder);

    void SetName(std::string_view name) { mName = name; }
    void SetTupleNum(size_t number) { mTupleNum = number; }



private:
    std::string mName;
    size_t mTupleNum;
    std::map<std::string, Attribute<int>, std::less<>> mAttributes;
};

using RelationRef = std::reference_wrapper<Relation>;

