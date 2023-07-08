#include <cassert>
#include <cstdlib>  // realloc
#include <cstring>  // memcpy
#include <iostream>
#include <stdexcept>
#include <string>

#include "minizip/unzip.h"
#include "tinyxml2/tinyxml2.cpp"

using namespace tinyxml2;
using std::runtime_error, std::string, std::cout, std::endl;

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
XMLElement* xmlNext(XMLElement* e) {
    return e->NextSiblingElement(e->Value());
}

// converts text:p to a plain string, regardless of formatting (=> handles XML text children and text:span children with an XML text child)
string stringifyTextPElem(XMLNode* n) {
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

int main(void) {
    // === load XML from .ods (which is a zip file internally) ===
    int lengthOfXmlData;
    char* buf = unzipToBuf("sampleInput.ods", "content.xml", &lengthOfXmlData);
    if (!buf) throw runtime_error("unzip failed");

    // === load XML ===
    XMLDocument doc;
    if (XML_SUCCESS != doc.Parse(buf, lengthOfXmlData)) throw runtime_error("loadfile failed");
    free(buf);

    // === locate first spreadsheet in XML hierarchy ===
    const char* targetElem = "office:document-content";
    XMLElement* e = doc.FirstChildElement(targetElem);
    if (!e) throw runtime_error(string("XML element '") + targetElem + "' not found at expected location");

    targetElem = "office:body";
    e = e->FirstChildElement(targetElem);
    if (!e) throw runtime_error(string("XML element '") + targetElem + "' not found at expected location");

    targetElem = "office:spreadsheet";
    e = e->FirstChildElement(targetElem);
    if (!e) throw runtime_error(string("XML element '") + targetElem + "' not found at expected location");

    XMLElement* table = e->FirstChildElement("table:table");
    if (!table) throw runtime_error("no table");  // need at least one
    while (table) {
        const char* tname = table->Attribute("table:name");
        if (!tname) throw runtime_error("no table name");
        cout << "$NEW_SHEET," << tname << endl;

        // === locate first row in XML hierarchy ===
        XMLElement* row = table->FirstChildElement("table:table-row");
        while (row) {
            string rowOut;
            size_t nRowRep = 1;
            const char* tnRowRep = row->Attribute("table:number-rows-repeated");
            if (tnRowRep != NULL)
                nRowRep = std::atol(tnRowRep);
            nRowRep = nRowRep < 1000 ? nRowRep : 1;  // heuristic hack to fix format error
            for (size_t ixRowRep = 0; ixRowRep < nRowRep; ++ixRowRep) {
                // === locate first cell in XML hierarchy ===
                XMLElement* cell = row->FirstChildElement("table:table-cell");
                while (cell) {
                    size_t nColRep = 1;
                    const char* tnColRep = cell->Attribute("table:number-columns-repeated");
                    if (tnColRep != NULL)
                        nColRep = std::atol(tnColRep);
                    nColRep = nColRep < 1000 ? nColRep : 1;  // heuristic hack to fix format error

                    // === extract value ===
                    XMLElement* text = cell->FirstChildElement("text:p");
                    const string textContent = text ? stringifyTextPElem(text) : "";
                    for (size_t ix = 0; ix < nColRep; ++ix) {
                        rowOut += textContent;
                        rowOut += ",";
                    }  // for nColRep
                    cell = xmlNext(cell);
                }  // while cell

                // === remove trailing separators ===
                while ((rowOut.size() > 0) && (rowOut.back() == ','))
                    rowOut = rowOut.substr(0, rowOut.size() - 1);
                cout << rowOut << endl;
            }  // for ixRowRep
            row = xmlNext(row);
        }  // while row
        table = xmlNext(table);
        cout << "$END_SHEET" << endl;
    }  // while table

    return 0;
}
