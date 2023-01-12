#include <iostream>
#include <stdexcept>
#include <string>

#include "tinyxml2.cpp"

using namespace tinyxml2;
using std::runtime_error, std::string, std::cout, std::endl;

//* traverse to next element of same type (name) e.g. table, row, cell in a spreadsheet */
XMLElement* xmlNext(XMLElement* e) {
    return e->NextSiblingElement(e->Value());
}
int main(void) {
    XMLDocument doc;
    if (XML_SUCCESS != doc.LoadFile("content.xml")) throw runtime_error("loadfile failed");
    XMLElement* e = doc.FirstChildElement("office:document-content")->FirstChildElement("office:body")->FirstChildElement("office:spreadsheet");
    if (!e) throw runtime_error("failed to parse xml hierarchy to office:spreadsheet level");
    XMLElement* table = e->FirstChildElement("table:table");
    if (!table) throw runtime_error("no table");  // need at least one
    while (table) {
        const char* tname = table->Attribute("table:name");
        if (!tname) throw runtime_error("no table name");
        cout << "$NEW_SHEET," << tname << endl;

        XMLElement* row = table->FirstChildElement("table:table-row");
        while (row) {
            size_t nRowRep = 1;
            const char* tnRowRep = row->Attribute("table:number-rows-repeated");
            if (tnRowRep != NULL)
                nRowRep = std::atol(tnRowRep);
            for (size_t ixRowRep = 0; ixRowRep < nRowRep; ++ixRowRep) {
                XMLElement* cell = row->FirstChildElement("table:table-cell");
                while (cell) {
                    size_t nColRep = 1;
                    const char* tnColRep = cell->Attribute("table:number-columns-repeated");
                    if (tnColRep != NULL)
                        nColRep = std::atol(tnColRep);

                    XMLElement* text = cell->FirstChildElement("text:p");
                    const char* tOut = text ? text->GetText() : "";
                    for (size_t ix = 0; ix < nColRep; ++ix) {
                        cout << tOut << ",";
                    }  // for nColRep
                    cell = xmlNext(cell);
                }  // while cell
                cout << endl;
            }  // for ixRowRep
            row = xmlNext(row);
        }  // while row
        table = xmlNext(table);
        cout << "$END_SHEET," << endl;
    }  // while table

    return 0;
}
