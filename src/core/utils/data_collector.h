#ifndef DATACOLLECTION_H
#define DATACOLLECTION_H

#include "param.h"
#include <chrono>
#include <unordered_map>
#include <list>
#include <iostream>
#include <type_traits>

class DataCollector {

public:

    /** Data Collector Management **/

    //Constructors and destructors
    DataCollector() = default;
    explicit DataCollector(std::string collection_name): name(collection_name) {setOutputPath(name);};
    ~DataCollector() = default;

    void setCollectionName(std::string name);
    void setOutputPath(std::string name) {output_path = Parameters::getCollectionPath() + name + ".csv";}

    //Dictionary management
    int addDictionary(std::string & tag, int type);
    int findIndex(std::string & tag, int type);
    std::string getValue(std::string & tag);            //Get a value (from tag), as a string
    std::string getDataStructure(std::string & tag);

    /** Initialize collection of a value **/

    //init (single value)
    template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    void init(std::string tag, T value) {initInteger(tag, value);}
    void initInteger(std::string tag, long long value);
    void init(std::string tag, std::string value);
    void init(std::string tag, double value);
    void initTime(std::string tag, double value = 0);

    void initTimes(std::vector<std::string> & tags);

    /** Collect a value **/

    //Collect (single value)
    template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    void collect(std::string tag, T value) {collectInteger(tag, value);}
    void collectInteger(std::string tag, long long value);
    void collect(std::string tag, std::string value);
    void collect(std::string tag, double value);

    /** Time profile management **/

    void startTime(std::string tag);
    void stopTime(std::string tag);
    void startGlobalTime();
    void stopGlobalTime();
    double getGlobalTime() {return times_cumulative[0];}
    double getGlobalTimeNow();

    /** Output management **/
    void setHeader();
    void saveRecord();
    void writeData();
    void print();
    void print(std::vector<std::string> tags);

    /** Reset values and clear data structures **/
    void reset(std::vector<std::string> tags);
    void resetText(int id) {text[id] = "";}
    void resetTime(std::string tag, double value = 0);

    void resetTexts() {std::fill(text.begin(), text.end(), "");}
    void resetTimes(double value = 0) {std::fill(times.begin(), times.end(), value);}
    void resetTimesCumulative(double value = 0) {std::fill(times_cumulative.begin(), times_cumulative.end(), value);}
    void resetIntegers(){ std::fill(integers.begin(), integers.end(), 0);}
    void resetDecimals(){std::fill(decimals.begin(), decimals.end(), 0);}

    void clearRecords() {records.clear(); records_mark.clear(); clearSubsetRecords();}
    void clear();

    /** Direct access to Data **/
    //For best performance

    //Text
    void setText(int id, std::string text) {this->text[id] = text;}
    std::string getText(int id) {return text[id];}

    //Times
    void startTime(int id){time_point[id] = std::chrono::high_resolution_clock::now();}
    void stopTime(int id);
    double getTime(int id) {return times[id];}
    double getTimeCumulative(int id) {return times_cumulative[id];}
    void resetTime(int id) {times[id] = 0;}
    void resetTimeCumulative(int id){times_cumulative[id] = 0;}

    //Integers
    void setInteger(int id, long long value){this->integers[id] = value;}
    long long getInt(int id) {return integers[id];}
    void resetInteger(int id) {integers[id] = 0;}

    //Decimals
    void setDecimal(int id, double value){decimals[id] = value;}
    double getDecimal(int id) {return decimals[id];}
    void resetDecimal(int id) {decimals[id] = 0;}

    //Records
    void setRecord(unsigned long long id, std::string record){records[id] = record;}
    void addRecord(std::string record){records.emplace_back(record); records_mark.emplace_back(false);}

    std::string getRecord(unsigned long long id){return records[id];}
    std::string getLastRecord(){return records.back();}

    void markRecord(unsigned long long id, bool mark = true){records_mark[id] = mark;}
    void markLastRecord(bool mark = true){records_mark.back() = mark;}

    bool getRecordMark(unsigned long long id){return records_mark[id];}
    bool getLastRecordMark(){return records_mark.back();}

    void addLastRecordToSubset(){records_subset.emplace_back(records.size()-1);}
    void markSubsetRecord(unsigned long long s, bool mark = true) {const unsigned long long id = records_subset[s]; records_mark[id] = mark;}

    std::vector<unsigned long long> * getSubsetRecords(){return &records_subset;}

    void resetRecordMarks(){std::fill(records_mark.begin(), records_mark.end(), false);}
    void clearSubsetRecords(){records_subset.clear();}

    //Helper functions
    template<typename Fn>
    void timed(const std::string& key, Fn fn) {
        startTime(key);
        fn();
        stopTime(key);
    }

    template<typename Fn>
    void timed(const int& key, Fn fn) {
        startTime(key);
        fn();
        stopTime(key);
    }

private:

    //Profiling
    std::string name;
    std::string output_path;

    //Dictionary:
    //Tag, pair(data structure type, index)
    //Data structure types: see constants
    std::unordered_map<std::string, std::pair<int, int>> dict;

    //Text
    std::vector<std::string> tag_text;
    std::vector<std::string> text;

    //Times
    std::vector<double> times, times_cumulative;
    std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>> time_point;
    std::vector<std::string> tag_times;

    //Integers
    std::vector<long long> integers;
    std::vector<std::string> tag_integers;

    //Decimals
    std::vector<double> decimals;
    std::vector<std::string> tag_decimals;

    //Data collection
    std::string header;
    std::vector<std::string> records;
    std::vector<bool> records_mark;             //one for each record
    std::vector<unsigned long long> records_subset;            //indices of records
};

#endif