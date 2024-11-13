#pragma once
#include <stdint.h>
#include <vector>
#include <assert.h>



class CRangeMgr {
public:

    class RangeHandler {
    public:
        virtual ~RangeHandler() {}
        virtual long AddRef() = 0;
        virtual long Release() = 0;
        virtual BOOL GetSize(uint64_t *pSize) = 0;
        virtual BOOL CalcDataSize(uint64_t* pSize) = 0;
        virtual BOOL Read(void* lpBuffer, DWORD nNumberOfBytesToRead, uint64_t* lpNumberOfBytesRead, BOOL* pIsFreeSpace) = 0;
        virtual void Close() = 0;
		virtual void SetExitFlag(BOOL* p) = 0;
    };

    struct Range {
        uint64_t start, end;
        bool include;
        RangeHandler* handler;
    };

private:
    std::vector<Range> mX;
    uint64_t mSize;
    uint64_t mPos;
public:
    CRangeMgr()
        : mSize(0)
        , mPos(0)
    {

    }

    void clear()
    {
        for (auto& i : mX) {
            if (i.handler) {
                i.handler->Close();
                i.handler->Release();
            }
        }
        mX.clear();
        mSize = 0;
        mPos = 0;
    }

    bool verify(uint64_t start, uint64_t end)
    {
        if (start >= end || end > mSize)
            return false;
        for (auto& i : mX)
        {
            if (start < i.end && end > i.end) {
                return false;
            }
            else if (start < i.start && end > i.start) {
                return false;
            }
            else if (start == i.start || end == i.end) {
                return false;
            }
            else if (start >= i.start && end <= i.end) {
                return false;
            }
        }
        return true;
    }

    void setSize(uint64_t sz)
    {
        mSize = sz;
    }

    uint64_t getSize()
    {
        return mSize;
    }

    bool insert(uint64_t start, uint64_t end, bool yesno, RangeHandler *pHandler = NULL)
    {
        if (!verify(start, end))
            return false;
        Range ex = { start, end, yesno, pHandler };
        std::vector<Range>::iterator p = mX.end();
        for (std::vector<Range>::iterator it = mX.begin(); it != mX.end(); it++) {
            if (start < it->start) {
                p = it;
                break;
            }
            else if (start >= it->end) {
                p = it + 1;
            }
        }
        mX.insert(p, ex);
        if (pHandler)
            pHandler->AddRef();
        return true;
    }

    bool split_range(uint64_t start, uint64_t end, uint64_t midpos, bool yesno, RangeHandler* pHandler = NULL)
    {
        for (size_t i = 0; i < mX.size(); i++)
        {
            if (start == mX[i].start && end == mX[i].end) {

                Range ex = { midpos, mX[i].end, yesno, pHandler };
                mX[i].end = midpos;

                mX.insert(mX.begin()+i+1, ex);
                if (pHandler)
                    pHandler->AddRef();
                return true;
            }
        }
        return false;
    }

    bool extend_range(uint64_t add_size, bool yesno, RangeHandler* pHandler = NULL)
    {
        uint64_t prev_size = mSize;
        mSize += add_size;

        if (!insert(prev_size, mSize, yesno, pHandler)) {
            mSize -= add_size;
            return false;
        }

        return true;
    }

    void insert_nomore()
    {
        std::vector<Range> yes;
        Range r = { 0, 0, true, nullptr };
        for (size_t i = 0; i < mX.size(); i++) {

            if (i == 0) {
                if (mX[i].start != 0) {
                    r.start = 0;
                    r.end = mX[i].start;
                    r.include = true;
                    yes.push_back(r);
                }
            }
            else {
                if (mX[i].start > mX[i - 1].end) {
                    r.start = mX[i - 1].end;
                    r.end = mX[i].start;
                    r.include = true;
                    yes.push_back(r);
                }
            }
        }

        if (mX.size() == 0) {
            r.start = 0;
            r.end = mSize;
            r.include = true;
            yes.push_back(r);
        }
        else if (mX[mX.size() - 1].end != mSize) {
            r.start = mX[mX.size() - 1].end;
            r.end = mSize;
            r.include = true;
            yes.push_back(r);
        }

        for (std::vector<Range>::iterator it = yes.begin(); it != yes.end(); it++) {
            if (!insert(it->start, it->end, it->include)) {
                assert(false);
            }
        }
    }

    bool getrange(uint64_t pos, Range& r)
    {
        if (pos >= mSize)
            return false;

        for (size_t i = 0; i < mX.size(); i++)
        {
            if (pos >= mX[i].start && pos < mX[i].end) {
                r = mX[i];
                return true;
            }
        }
        return false;
    }

    bool calcSize(uint64_t *pSize)
    {
        uint64_t sz = 0;
        for (size_t i = 0; i < mX.size(); i++)
        {
            uint64_t j;
            if (mX[i].include) {
                if (mX[i].handler) {
                    if (!mX[i].handler->CalcDataSize(&j))
                        return false;
                }
                else {
                    j = mX[i].end - mX[i].start;
                }
                sz += j;
            }
        }
        *pSize = sz;
        return true;
    }
};
