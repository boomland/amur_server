#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <initializer_list>
#include <math.h>

enum AlignType {
        AT_LEFT,
        AT_CENTER,
        AT_RIGHT
};

struct PrettyColumn {
    std::string name;
    size_t max_len;
    AlignType align_type;
    size_t actual_len;
    
    PrettyColumn() : name(""), max_len(50), align_type(AT_LEFT), actual_len(0) {}
    PrettyColumn(const std::string& _name, size_t _max_len, AlignType _al_type)
        : name(_name), max_len(_max_len), align_type(_al_type) {}
    PrettyColumn(const PrettyColumn& _column) 
        : name(_column.name), max_len(_column.max_len), align_type(_column.align_type),
            actual_len(_column.actual_len) {}
};

template<typename T>
std::string myToString(T cont) {
    std::ostringstream iss;
    iss << cont;
    return iss.str();
}

struct PrettyContent : public std::string {
   
    PrettyContent() : std::string() {}
    
    template<typename T>
    PrettyContent(T cont) : std::string(myToString(cont)) {}
};

struct PrettyRow {
    PrettyRow(const std::initializer_list<PrettyContent> lst) {
        values.resize(lst.size());
        int i = 0;
        for (auto it = lst.begin(); it != lst.end(); ++it) {
            values[i++] = *it;
        }
    }
    
    PrettyRow(const PrettyRow& row) {
        values = row.values;
        names = row.names;
    }
    
    void setNames(std::map<std::string, int> *_names) {
        names = _names;
    }
    
    std::string& operator[](size_t idx) {
        return values[idx];
    }
    
    std::string& operator[](const std::string& name) {
        return values[(*names)[name]];
    }
    
    
    
    std::vector<PrettyContent> values;
    std::map<std::string, int>   *names;
};


class PrettyTable {
 public:
    PrettyTable() {}
    
    PrettyTable(const std::string& header,
                const std::vector<PrettyColumn>& columns)
    {
        name_ = header;
        columns_ = columns;
        for (int i = 0; i != columns.size(); ++i) {
            names[columns[i].name] = i;
        }
    }
    
    void addRow(const PrettyRow& row) {
        data_.push_back(row);
        data_[data_.size() - 1].setNames(&names);
    }
    
    PrettyRow& operator[](size_t idx) {
        return data_[idx];
    }

    size_t getTableLength() {
        for (size_t i = 0; i != colsCount(); ++i) {
            columns_[i].actual_len = std::min(columns_[i].name.size(), columns_[i].max_len);
        }
        for (size_t i = 0; i != rowsCount(); ++i) {
            for (size_t j = 0; j != colsCount(); ++j) {
                size_t cur_len = data_[i][j].size();
                columns_[j].actual_len = std::max(columns_[j].actual_len,
                                                  std::min(columns_[j].max_len,
                                                           cur_len));
            }
        }
        size_t sum_size = 1;
        for (size_t i = 0; i != colsCount(); ++i) {
            sum_size += 3 + columns_[i].actual_len;
        }
		return std::max(sum_size, name_.size() + 2);
    }

    std::string dump() {
        size_t sum_size = getTableLength();
        std::string result;
        result += printLine('=', sum_size) + '\n';
        result += '=' + alignText(name_, sum_size - 2, AT_CENTER) + "=\n";
        result += printLine('=', sum_size) + '\n';
        result += printRow(0, 1, sum_size);

        for (size_t i = 0; i != rowsCount(); ++i) {
            result += printRow(i, 0, sum_size);
        }
	
        return result;
    }

    std::string printRow(int i, bool is_header, size_t sum_size) {
        std::string result;
        std::vector<std::vector<std::string>> d(
            1, std::vector<std::string>(colsCount()));
        
        for (int j = 0; j != colsCount(); ++j) {
            if (is_header)
                d[0][j] = columns_[j].name;
            else
                d[0][j] = data_[i][j];
        }

        int real_rows = 1;

        for (int num_col = 0; num_col != colsCount(); ++num_col) {
            int sz;
            if (is_header)
                sz = columns_[num_col].name.size();
            else
                sz = data_[i][num_col].size();
            int cur = ceil(((double)sz) / (double)columns_[num_col].actual_len);
            real_rows = std::max(real_rows, cur);
        }

        d.resize(real_rows);

        for (int k = 0; k != real_rows; ++k) {
            if (k != real_rows - 1) {
                d[k + 1] = std::vector<std::string>(colsCount(), "");
                for (int num_col = 0; num_col != colsCount(); ++num_col) {
                    std::string stay = d[k][num_col].substr(0,
                        columns_[num_col].actual_len);
                    std::string overflow = "";
                    if (d[k][num_col].size() > columns_[num_col].actual_len) {
                        overflow = d[k][num_col].substr(columns_[num_col].actual_len,
                            std::string::npos);
                    }
                    d[k][num_col] = stay;
                    d[k + 1][num_col] = overflow;
                }
            }
            result += printRowInternal(d[k]);
        }
        result += printLine('-', sum_size) + '\n';
        return result;
    }

    std::string printRowInternal(const std::vector<std::string>& d) {
        std::string result = "| ";
        for (int j = 0; j != colsCount(); ++j) {
            result += alignText(d[j],
                columns_[j].actual_len, columns_[j].align_type) + " | ";
        }
        return result + '\n';
    }

    std::string printLine(char c, int n) {
        std::string result;
        for (int i = 0; i < n; ++i) {
            result += c;
        }
        return result;
    }

    std::string alignText(const std::string& text, int n, AlignType at) {
        std::string result;
        if (at == AT_LEFT) {
            result += text;
            result += printLine(' ', n - text.size());
        }
        if (at == AT_RIGHT) {
            result += printLine(' ', n - text.size());
            result += text;
        }
        if (at == AT_CENTER) {
            int pad = n - text.size();
            int left_pad = pad / 2;
            result += printLine(' ', left_pad);
            result += text;
            result += printLine(' ', pad - left_pad);
        }
        return result;
    }

    size_t rowsCount() const {
        return data_.size();
    }
    size_t colsCount() const {
        return columns_.size();
    }
    
    
 private:
    std::vector<PrettyColumn> columns_;
    std::string name_;
    std::vector<PrettyRow> data_;
    std::map<std::string, int> names;
};


/*

    For example of using:

int main() {
    PrettyTable table("myFirstTable", 
        {
            {"text_col", 50, AT_CENTER},
            {"double_col", 50, AT_CENTER},
            {"long_col", 25, AT_CENTER},
            {"bool_col1", 50, AT_CENTER},
            {"bool_col2", 50, AT_CENTER}
        });
    
    table.addRow({
        "abracadabra",
        1.45645,
        "Some extra long text, that does not fit in one row and should be overflowed",
        true,
        false
    });
    table.addRow({
        "Columns",
        "at different",
        "rows shouldn't have same types",
        1.54,
        "=)"
    });
    std::cout << table.dump();
    // More: get content of row #i, column named "text_col"
    std::cout << table[i]["text_col"] << std::endl;
    // The same:
    std::cout << table[i][0] << std::endl;
    // Also you can change content of cell like this:
    table[i]["long_col"] = "abracadabra_v2";
    return 0;
}
*/
