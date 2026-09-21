#include "pch.h"
#include "Disks.h"
#include "json.hpp"
#include <vector>
#include "ImageMaker.h"
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <atlbase.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <locale.h>
#include <iomanip>
#include "human-readable.h"
#include "langstr.h"
#include "osutils.h"
#include "ProgressEstimate.h"

using json = nlohmann::ordered_json;

bool ask_numbers(const wchar_t *p, bool allow_empty, std::vector<int> &v)
{
    int i;
    std::string in;
    v.clear();
    std::wcout << p;
    std::getline(std::cin, in);
    while (!allow_empty && in.size() == 0)
        std::getline(std::cin, in);
    std::stringstream ss(in);
    char c;
    for (;;) {
        c = ss.peek();
        if (c == EOF)
            return true;
        else if (c == ',' || c == ' ')
            ss.ignore();
        else if (!isdigit(c))
            return false;
        else
        {
            ss >> i;
            v.push_back(i);
        }
    }
}


int main(int argc, char**argv)
{
    LANGID lid = GetUserDefaultUILanguage();
    if (lid == 2052) {
        setlocale(LC_ALL, "chs");
        std::wcout.imbue(std::locale("chs"));
    }
    EnableBackupPrivilege();
    std::string text;
    json jdisks;

    int i = 0;
    char buf[256];
    text = enumPartitions();
    jdisks = json::parse(text);

    if (jdisks.size() == 0)
    {
        fwprintf(stderr, L"%s\n", LSTRW(RID_nodisks));
        return -1;
    }

    if (argc == 2 && !_stricmp(argv[1], "--dumpinfo")) {
        fprintf(stdout, "%s", text.c_str());
        return 0;
    }

    std::wcout << std::left << std::setw(5) << LSTRW(RID_Disk) << std::setw(34) << LSTRW(RID_Caption) << std::setw(8) << LSTRW(RID_BusType) << std::setw(21) << LSTRW(RID_Size) << std::endl;

    for (i = 0; i < (int)jdisks.size(); i++)
    {
        json disk = jdisks[i];
        json property = disk["property"];
        int index = disk["index"];
        std::string Index = std::to_string(index);

        std::string Caption = property["VendorId"].get<std::string>() + " " + property["ProductId"].get<std::string>();
        while (Caption.size() && *Caption.begin() == ' ') {
            Caption.erase(Caption.begin());
        }
        std::string BusType = property["BusType"].get<std::string>();
        uint64_t DiskSize = property["DiskSize"];
        std::string strDiskSize = std::to_string(DiskSize);
        std::string strDiskSizeH = calculateSize1000(DiskSize, buf, sizeof(buf));

        std::wcout << std::left << std::setw(5) << (LPCWSTR)CA2W(Index.c_str())
            << std::setw(36) << (LPCWSTR)CA2W(Caption.c_str(), CP_UTF8)
            << std::setw(8) << (LPCWSTR)CA2W(BusType.c_str())
            << std::setw(19) << (LPCWSTR)CA2W((strDiskSize + "(" + strDiskSizeH + ")").c_str())
            << std::endl;
    }

    std::vector<int> askres;
    CImageMaker maker;
    int diskno;
    ask_numbers(LSTRW(RID_ChooseDisk), false, askres);
    if (1 != askres.size()) {
        fwprintf(stderr, L"%s\n", LSTRW(RID_InvalidInput));
        return -1;
    }
    diskno = askres[0];

    json disk;
    for (i = 0; i < (int)jdisks.size(); i++)
    {
        disk = jdisks[i];
        if (diskno == disk["index"])
            break;
    }
    if (i == (int)jdisks.size()) {
        fwprintf(stderr, L"%s\n", LSTRW(RID_InvalidInput));
        return -1;
    }

    json property = disk["property"];
    json partitions = disk["partitions"];
    if (partitions.size() == 0)
    {
        fwprintf(stderr, L"%s\n", LSTRW(RID_NoPartitions));
        return -1;
    }

    std::wcout << std::left << std::setw(6) << LSTRW(RID_partition) << std::setw(24) << LSTRW(RID_label) << std::right << std::setw(15) << LSTRW(RID_Size) << std::setw(4) << LSTRW(RID_Usage) << std::setw(13) << LSTRW(RID_1stsector) << std::endl;

    for (i = 0; i < (int)partitions.size(); i++)
    {
        json part = partitions[i];
        int number = part["number"];
        uint64_t length = part["length"];
        if (!length)
            continue;
        uint64_t offset = part["offset"];
        uint64_t length_free = part["free"];
        std::string label = part["label"].get<std::string>();
        std::string mount = part["mount"].get<std::string>();
        std::string item_name;
        int BytesPerSector = property["BytesPerSector"];

        item_name = label;
        if (mount.size()) {
            item_name += "(";
            item_name += mount;
            item_name += ")";
        }

        std::string strlength = calculateSize1024(length, buf, sizeof(buf));
        std::string used;
        if (length_free) {
            int percent_used = (int)((length - length_free) * 100 / length);
            snprintf(buf, sizeof(buf), "%d%%", percent_used);
            used = buf;
        }

        std::wcout << std::left << std::setw(8) << number
            << std::setw(26) << (LPCWSTR)CA2W(item_name.c_str(), CP_UTF8)
            << std::right << std::setw(17) << (LPCWSTR)CA2W(strlength.c_str())
            << std::setw(6) << (LPCWSTR)CA2W(used.c_str())
            << std::setw(17) << offset / BytesPerSector
            << std::endl;
    }

    std::vector<int> vect_excludes;

    while (!ask_numbers(LSTRW(RID_InputPartNumbersToBeExcluded), true, vect_excludes))
    {
        fwprintf(stderr, L"%s\n", LSTRW(RID_InvalidInput));
    }

    if (!maker.setSource(diskno, property["DiskSize"])) {
        fwprintf(stderr, L"%s: %s, %s\n", LSTRW(RID_error), LSTRW(RID_OpenSourceError), maker.lasterr().c_str());
        return -1;
    }

    for (int no : vect_excludes) {
        json part;
        for (i = 0; i < (int)partitions.size(); i++)
        {
            part = partitions[i];
            if (no == part["number"])
                break;
        }
        if (i == (int)partitions.size()) {
            fwprintf(stderr, L"%s\n", LSTRW(RID_InvalidPartNum));
            return -1;
        }
        uint64_t offset = part["offset"];
        uint64_t length = part["length"];
        int BytesPerSector = property["BytesPerSector"];

        if (!maker.addExcludeRange(offset, offset + length, 1)) {
            fwprintf(stderr, L"%s: %s\n", LSTRW(RID_error), maker.lasterr().c_str());
            return -1;
        }
        fwprintf(stdout, LSTRW(RID_ExcludedParts), no, offset / BytesPerSector, (offset + length) / BytesPerSector);

    }

    std::string strDest;
    while (TRUE) {
        strDest.clear();
        std::wcout << LSTRW(RID_SetOutputPath);
        while (strDest.size() == 0)
            std::getline(std::cin, strDest);

        if (!maker.setDest(CA2W(strDest.c_str()), true)) {
            fwprintf(stderr, L"%s: %s\n", LSTRW(RID_error), maker.lasterr().c_str());
            continue;
        }
        break;
    }

    if (0 != _wcsicmp(L"DD", maker.getFormat().c_str())) {
        std::vector<int> vect_options;
        DWORD dwOptions = 0;
        while (!ask_numbers(LSTRW(RID_SetImageOptions), true, vect_options)) {
            fwprintf(stderr, L"%s\n", LSTRW(RID_InvalidInput));
        }
        for (auto opt : vect_options) {
            dwOptions |= opt;
        }
        maker.setOptions(dwOptions);
    }

    std::ofstream savejson;
    savejson.open(strDest+".json");
    savejson << disk.dump(2);
    savejson.close();

    class Cprogress : public IImageMakerCallback
    {
    public:
        DWORD mTickBegin;
        DWORD mTick;
        uint64_t position_prev, data_copied_prev;
        uint64_t _position, _disksize;
        double m_smoothedDataSpeed;
        double m_smoothedScanSpeed;
        CImageMaker& m_maker;

        Cprogress(CImageMaker& maker) : m_maker(maker) {
            ResetTiming();
        }
        void ResetTiming() {
            mTickBegin = mTick = GetTickCount();
            position_prev = data_copied_prev = 0;
            _position = _disksize = 0;
            m_smoothedDataSpeed = 0.0;
            m_smoothedScanSpeed = 0.0;
        }
        virtual void ImageMaker_OnStart()
        {
            ResetTiming();
            fwprintf(stderr, LSTRW(RID_OnStart));
        }

        virtual void ImageMaker_OnCopied(uint64_t position, uint64_t disksize, uint64_t data_copied, uint64_t data_space_size) {
            char txt_cp[128];
            char txt_speed[128];
            char used[128];
            char lefttime[128];
            DWORD now = GetTickCount();
            DWORD t = now - mTick;
            _position = position;
            _disksize = disksize;
            if (t >= 1000) {
                mTick = now;
                calculateSize1024(position, txt_cp, sizeof(txt_cp));

                uint64_t inc_pos = (position >= position_prev) ? (position - position_prev) : 0;
                uint64_t inc_data = (data_copied >= data_copied_prev) ? (data_copied - data_copied_prev) : 0;
                double inst_pos_speed = (double)inc_pos * 1000.0 / t;
                double inst_data_speed = (double)inc_data * 1000.0 / t;

                if (m_smoothedScanSpeed <= 0.0) {
                    m_smoothedScanSpeed = inst_pos_speed;
                } else {
                    m_smoothedScanSpeed = 0.25 * inst_pos_speed + 0.75 * m_smoothedScanSpeed;
                }

                if (inc_data > 0) {
                    if (m_smoothedDataSpeed <= 0.0) {
                        m_smoothedDataSpeed = inst_data_speed;
                    } else {
                        m_smoothedDataSpeed = 0.25 * inst_data_speed + 0.75 * m_smoothedDataSpeed;
                    }
                } else {
                    if (m_smoothedDataSpeed > 0.0) {
                        m_smoothedDataSpeed = 0.95 * m_smoothedDataSpeed;
                    }
                }

                DWORD total_elapsed_ms = mTick - mTickBegin;
                if (total_elapsed_ms == 0) total_elapsed_ms = 1;
                double total_elapsed_sec = (double)total_elapsed_ms / 1000.0;
                double avg_data_speed = (double)data_copied / total_elapsed_sec;
                double avg_pos_speed = (double)position / total_elapsed_sec;

                double est_data_speed = (m_smoothedDataSpeed > 0.0)
                    ? (0.7 * m_smoothedDataSpeed + 0.3 * avg_data_speed)
                    : avg_data_speed;
                double est_pos_speed = (m_smoothedScanSpeed > 0.0)
                    ? (0.7 * m_smoothedScanSpeed + 0.3 * avg_pos_speed)
                    : avg_pos_speed;

                uint64_t est_sec = 0;
                bool sparse_output = m_maker.GetWorkingMode() == CImageMaker::Mode_VD;
                bool has_estimate = EstimateRemainingSeconds(sparse_output,
                    position, disksize, data_copied, data_space_size,
                    inc_data > 0 ? est_data_speed : 0.0, inc_pos > 0 ? est_pos_speed : 0.0, est_sec);

                double display_speed = (sparse_output && inc_data > 0 && m_smoothedDataSpeed > 0.0)
                    ? m_smoothedDataSpeed : m_smoothedScanSpeed;

                calculateSize1024((uint64_t)display_speed, txt_speed, sizeof(txt_speed));
                calcseconds((mTick-mTickBegin)/1000, used, sizeof(used));
                if (has_estimate)
                    calcseconds(est_sec, lefttime, sizeof(lefttime));
                else
                    strcpy_s(lefttime, sizeof(lefttime), "--");
                fprintf(stderr, "%I64d bytes (%s) copied, %s, EST. %s, %s/s      \r", position, txt_cp, used, lefttime, txt_speed);
                position_prev = position;
                data_copied_prev = data_copied;
            }
        }
        virtual void ImageMaker_OnEnd(int err_code) {
            char txt_cp[128];
            char used[128];
            calculateSize1024(_position, txt_cp, sizeof(txt_cp));
            mTick = GetTickCount();
            calcseconds((mTick - mTickBegin) / 1000, used, sizeof(used));
            fprintf(stderr, "\n%I64d bytes (%s) copied, %s\n", _position, txt_cp, used);

            if (err_code)
                fwprintf(stderr, LSTRW(RID_ConvertorThreadError), err_code);
        }

    }progress(maker);

    maker.setCallback(&progress);
    if (!maker.start()) {
        fwprintf(stderr, LSTRW(RID_ErrorInfo), maker.lasterr().c_str());
        return -1;
    }

    if (!maker.wait(INFINITE)) {
        fwprintf(stderr, LSTRW(RID_ErrorInfo), maker.lasterr().c_str());
        return -1;
    }

    if (0 != maker.getConvertorExitCode()) {
        fwprintf(stderr, LSTRW(RID_ConvertorError), maker.getConvertorExitCode(), maker.lasterr().c_str());
        return -1;
    }

    fwprintf(stderr, L"\n");
    fwprintf(stderr, LSTRW(RID_GOOD));
    return 0;
}
