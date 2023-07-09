#include <cassert>
#include <cstdlib>  // realloc
#include <cstring>  // memcpy
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include "minizip/unzip.h"
#include "tinyxml2/tinyxml2.cpp"

using namespace tinyxml2;
using std::runtime_error, std::string, std::cout, std::endl, std::map;

/* Loads "fileToExtract" from "zipfile". Returns buffer with contents or NULL, if failed.
   Use free() on buffer to deallocate.
   length returns the number of bytes. Contents are null-terminated.
*/
char* unzipToBuf(const char* zipfile, const char* fileToExtract, int* length) {
    unzFile uf = unzOpen64(zipfile);
    if (!uf) return NULL;
    if (unzLocateFile(uf, fileToExtract, /*case sensitive*/ 0) != UNZ_OK) return NULL;
    if (unzOpenCurrentFilePassword(uf, /*password*/ NULL)) return NULL;

    char chunk[65536];
    int nRead = 0;
    char* retBuf = NULL;
    while (1) {
        int nBytes = unzReadCurrentFile(uf, chunk, sizeof(chunk));
        if (nBytes < 0) {                        // error
            retBuf = (char*)realloc(retBuf, 0);  // free
            return NULL;
        }

        if (nBytes == 0)
            break;
        retBuf = (char*)realloc(retBuf, nRead + nBytes + /*null termination*/ 1);
        memcpy(/*dest*/ retBuf + nRead, /*src*/ chunk, nBytes);
        *(retBuf + nRead + nBytes) = 0;  // null termination
        nRead += nBytes;
    }

    *length = nRead;
    return retBuf;
}

//* traverse to next element of same type (name) e.g. table, row, cell in a spreadsheet */
const XMLElement* xmlNext(const XMLElement* e) {
    return e->NextSiblingElement(e->Value());
}

// converts text:p to a plain string, regardless of formatting (=> handles XML text children and text:span children with an XML text child)
string stringifyTextPElem(const XMLNode* n) {
    assert(n);
    assert(n->Value() == string("text:p"));
    n = n->FirstChild();
    string r;
    while (n) {
        const string v = n->Value();
        if (n->ToText())  // is this a text node?
            r = r + v;    // if so, Value() is the actual text
        else if (v == "text:span")
            if (n->FirstChild())
                if (n->FirstChild()->ToText() && n->FirstChild()->Value())
                    r = r + n->FirstChild()->Value();
        n = n->NextSibling();
    }
    return r;
}

const XMLNode* safeFirstChildElem(const XMLNode* e, const char* targetElem) {
    e = e->FirstChildElement(targetElem);
    if (!e) throw runtime_error(string("XML element '") + targetElem + "' not found at expected location");
    return e;
}

// returns table row indexed by column number
map<size_t, string> parseRow(const XMLElement* row) {
    assert(row);
    assert(row->Value() == string("table:table-row"));

    map<size_t, string> r;
    // === locate first cell in XML hierarchy ===
    const XMLElement* cell = row->FirstChildElement("table:table-cell");
    size_t ixCol = 0;
    while (cell) {
        size_t nColRep = 1;
        const char* tnColRep = cell->Attribute("table:number-columns-repeated");
        if (tnColRep != NULL)
            nColRep = std::atol(tnColRep);
        //        nColRep = nColRep < 1000 ? nColRep : 1;  // heuristic hack to fix format error

        // === extract value ===
        const XMLElement* text = cell->FirstChildElement("text:p");
        const string textContent = text ? stringifyTextPElem(text) : "";
        if (textContent.size() > 0) {
            for (size_t ix = 0; ix < nColRep; ++ix) {
                auto v = r.insert({ixCol++, textContent});
                assert(/*insertion succeeded */ v.second);
            }
        } else {
            ixCol += nColRep;
        }
        cell = xmlNext(cell);
    }  // while cell
    return r;
}

// returns map hierarchy for one table indexed by row number then column number
map<size_t, map<size_t, string>> parseTable(const XMLElement* e) {
    assert(e);
    assert(e->Value() == string("table:table"));

    map<size_t, map<size_t, string>> r;
    // === locate first row in XML hierarchy ===
    const XMLElement* row = e->FirstChildElement("table:table-row");
    size_t ixRow = 0;
    while (row) {
        string rowOut;
        size_t nRowRep = 1;
        const char* tnRowRep = row->Attribute("table:number-rows-repeated");
        if (tnRowRep != NULL)
            nRowRep = std::atol(tnRowRep);
        //        nRowRep = nRowRep < 1000 ? nRowRep : 1;  // heuristic hack to fix format error

        map<size_t, string> rowMap = parseRow(row);
        if (rowMap.size() > 0) {
            for (size_t ixRowRep = 0; ixRowRep < nRowRep; ++ixRowRep) {
                auto v = r.insert({ixRow++, rowMap});
                assert(/*insertion succeeded*/ v.second);
            }
        } else {
            ixRow += nRowRep;
        }
        row = xmlNext(row);
    }  // while row
    return r;
}

// returns map hierarchy indexed by sheet name/row number/column number
map<string, map<size_t, map<size_t, string>>> ods2txt_sparse(const string& fname) {
    // === load XML from .ods (which is a zip file internally) ===
    int lengthOfXmlData;
    char* buf = unzipToBuf(fname.c_str(), "content.xml", &lengthOfXmlData);
    if (!buf) throw runtime_error("unzip failed");

    // === load XML ===
    XMLDocument doc;
    if (XML_SUCCESS != doc.Parse(buf, lengthOfXmlData)) throw runtime_error("loadfile failed");
    free(buf);

    // === locate first spreadsheet in XML hierarchy ===
    const XMLNode* e = &doc;
    e = safeFirstChildElem(e, "office:document-content");
    e = safeFirstChildElem(e, "office:body");
    e = safeFirstChildElem(e, "office:spreadsheet");
    const XMLElement* table = safeFirstChildElem(e, "table:table")->ToElement();
    if (!table) throw runtime_error("document contains no tables!");

    map<string, map<size_t, map<size_t, string>>> r;

    while (table) {
        const char* tname = table->Attribute("table:name");
        if (!tname) throw runtime_error("no table name");
        const auto& v = r.insert({tname, parseTable(table)});
        assert(/*insertion succeeded*/ v.second);
        table = xmlNext(table);
    }  // while table
    return r;
}

int main(void) {
    const string sepCol(",");
    const string sepRow("\n");
    map<string, map<size_t, map<size_t, string>>> bookData = ods2txt_sparse("sampleInput.ods");

    // === iterate over sheets ===
    for (const auto& tableInBook : bookData) {
        const string& tableName = tableInBook.first;
        const map<size_t, map<size_t, string>>& tableData = tableInBook.second;

        cout << "$NEW_SHEET," << tableName << sepRow;
        size_t lastTerminatedIxRow = 0;

        // === iterate over rows ===
        for (const auto& rowInSheet : tableData) {
            size_t ixRow = rowInSheet.first;
            const map<size_t, string>& rowData = rowInSheet.second;
            if (rowData.size() < 1) continue;  // defer output of possibly trailing separators

            // === write row separators ===
            for (size_t ix = lastTerminatedIxRow; ix < ixRow; ++ix)
                cout << sepRow;
            lastTerminatedIxRow = ixRow;
            size_t lastTerminatedIxCol = 0;

            // === iterate over columns ===
            for (auto& cellInRow : rowData) {
                size_t ixCol = cellInRow.first;
                const string& cellText = cellInRow.second;
                if (cellText.size() < 1) continue;  // defer output of possibly trailing separators

                // === write column separators ===
                for (size_t ix = lastTerminatedIxCol; ix < ixCol; ++ix)
                    cout << sepCol;
                lastTerminatedIxCol = ixCol;

                // === write cell content ===
                cout << cellText;
            }
            cout << sepRow;
            lastTerminatedIxRow = ixRow;
        }  // for rowInSheet

        cout << "$END_SHEET" << sepRow;
    }  // for table in sheet

    return 0;
}