#pragma once

#include <algorithm>
#include <assert.h>
#include <cstddef>
#include <iostream>
#include <numeric>
#include <set>
#include <string>
#include <vector>

#include "Relation.h"

struct Range
{
    size_t st, ed;

    size_t Length() const { return ed - st; }

    bool Valid() { return st < ed; }
};

inline Range MergeRange(Range r1, Range r2)
{
    return Range{std::max(r1.st, r2.st), std::min(r1.ed, r2.ed)};
}

using RangeTuple = Range*;


class RangeTable
{
public:
    RangeTable(size_t relationNum, size_t maxTupleNum)
        : mRelationNum(relationNum), mTupleNum(0), mRanges(nullptr)
    {
        mRanges = new Range[mRelationNum * maxTupleNum];
    }

    RangeTable(RangeTable&& table)
        : mRelationNum(table.mRelationNum), mTupleNum(table.mTupleNum), mRanges(table.mRanges), mRelationIndices(std::move(table.mRelationIndices))
    {
        table.mRanges = nullptr;
    }

    ~RangeTable() { 
        if (mRanges)
            delete[] mRanges; 
        mRanges = nullptr;
    }

    // Get the last unused tuple and increase one to mTupleNum
    RangeTuple AcquireTuple()
    {
        RangeTuple tuple = &mRanges[mTupleNum * mRelationNum];
        mTupleNum++;

        return tuple;
    }

    RangeTable& operator=(RangeTable&& table)
    {
        mRelationNum = table.mRelationNum;
        mTupleNum = table.mTupleNum;
        mRanges = table.mRanges;

        table.mRanges = nullptr;

        return *this;
    }

    RangeTuple operator[](size_t index)
    {
        return &(mRanges[index * mRelationNum]);
    }

    size_t Length() const { return mTupleNum; }

    size_t Width() const { return mRelationNum; }

    void Print()
    {
        for (int i = 0; i < mTupleNum; i++)
        {
            std::cout << '[';
            for (int j = 0; j < mRelationNum; j++)
            {
                Range& range = mRanges[i * mRelationNum + j];
                std::cout << "(" << range.st << "," << range.ed << ")  ";
            }
            std::cout << ']' << std::endl;
        }
    }

    std::vector<size_t> LazySort(std::vector<Relation>& relations, size_t relationIndex, std::string attr)
    {
        std::vector<size_t> seq(mTupleNum);
        std::iota(seq.begin(), seq.end(), 0);

        auto attRef = relations[relationIndex][attr];

        std::sort(seq.begin(), seq.end(),
            [&](size_t id1, size_t id2)
            {
                size_t r1 = operator[](id1)[relationIndex].st;
                size_t r2 = operator[](id2)[relationIndex].st;
                return attRef.get()[r1] < attRef.get()[r2];
            }
        );

        return seq;
    }

    std::vector<size_t> LazySort(std::vector<Relation>& relations, std::vector<size_t>& relationIndices, std::vector<std::string>& attrVec)
    {
        std::vector<size_t> seq(mTupleNum);
        std::iota(seq.begin(), seq.end(), 0);

        std::vector<AttributeRef<int>> attrRefVec;
        for (size_t i = 0; i < relationIndices.size(); i++)
        {
            auto& rel = relations[relationIndices[i]];
            attrRefVec.emplace_back(rel[attrVec[i]]);
        }

        auto cmp = [&](size_t id1, size_t id2) {
                for (size_t i = 0; i < attrRefVec.size(); i++)
                {
                    size_t r1 = operator[](id1)[relationIndices[i]].st;
                    size_t r2 = operator[](id2)[relationIndices[i]].st;
                    int v1 = attrRefVec[i].get()[r1]; 
                    int v2 = attrRefVec[i].get()[r2];
                    if (v1 != v2)
                        return v1 < v2;
                }
                return false;
        };

        std::sort(seq.begin(), seq.end(), cmp);

        return seq;
    }

    bool ExistRel(size_t relIndex) const
    {
        return mRelationIndices.count(relIndex) > 0;
    }

    void AddRelName(size_t relIndex)
    {
        mRelationIndices.insert(relIndex);
    }
    
    std::set<size_t>& GetRelIndices()
    {
        return mRelationIndices;
    }

private:
    size_t mRelationNum;
    size_t mTupleNum;

    Range* mRanges;

    std::set<size_t> mRelationIndices; // 
};

using RangeTableRef = std::reference_wrapper<RangeTable>;


struct RangeTableIterator
{
    RangeTableIterator(std::vector<size_t>& iter)
        : mMaxIndex(iter), mCurrentIndex(iter.size(), 0), mCompleted(false)
    {}

    std::vector<size_t>& Get()
    {
        return mCurrentIndex;
    }

    void operator++(int)
    {
        int tableIndex = mCurrentIndex.size() - 1;
        int carry = 1;
        do
        {
            mCurrentIndex[tableIndex] += carry;
            carry = 0;
            if (mCurrentIndex[tableIndex] >= mMaxIndex[tableIndex])
            {
                mCurrentIndex[tableIndex] -= mMaxIndex[tableIndex];
                carry = 1;
                tableIndex--;
            }
        } while (carry and tableIndex >= 0);
        
        if (carry and tableIndex < 0)
            mCompleted = true;
    }

    operator bool() const
    {
        return !mCompleted;
    }

    std::vector<size_t> mMaxIndex;
    std::vector<size_t> mCurrentIndex;
    bool mCompleted;
};


struct RangeVecIterator
{
    RangeVecIterator(std::vector<Range>& rangeVec)
        : mRange(rangeVec), mCurrentIndex(rangeVec.size(), 0), mCompleted(false)
    {
        for (size_t i = 0; i < rangeVec.size(); i++)
            mCurrentIndex[i] = mRange[i].st;
    }

    const std::vector<size_t>& Get()
    {
        return mCurrentIndex;
    }

    void operator++(int)
    {
        int tableIndex = mCurrentIndex.size() - 1;
        int carry = 1;
        do
        {
            mCurrentIndex[tableIndex] += carry;
            carry = 0;
            if (mCurrentIndex[tableIndex] >= mRange[tableIndex].ed)
            {
                mCurrentIndex[tableIndex] = mRange[tableIndex].st;
                carry = 1;
                tableIndex--;
            }
        } while (carry and tableIndex >= 0);
        
        if (carry == 1 and tableIndex < 0)
            mCompleted = true;
    }

    operator bool()
    {
        return !mCompleted;
    }
    
    std::vector<Range>& mRange;
    std::vector<size_t> mCurrentIndex;
    bool mCompleted;
};


inline size_t ShortestRelIdInRangeTuple(RangeTuple rtuple, std::vector<size_t>& relIdVec)
{
    size_t shortRelId = relIdVec[0];
    for (size_t relId : relIdVec)
        if (rtuple[relId].Length() < rtuple[shortRelId].Length())
            shortRelId = relId;
    return shortRelId;
}