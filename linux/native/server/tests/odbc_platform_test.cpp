#include "../odbc_platform.hpp"
#include <cassert>
#include <iostream>
class IndicatorProbe:public CSqlBase {
public:
    using ColumnIndicator=std::remove_extent_t<decltype(m_liCOLS)>;
    using ParameterIndicator=std::remove_extent_t<decltype(m_liPARAMS)>;
};
int main() {
    static_assert(sizeof(SQLINTEGER)==4 && sizeof(SQLLEN)==8);
    static_assert(std::is_same_v<IndicatorProbe::ColumnIndicator,SQLLEN>);
    static_assert(std::is_same_v<IndicatorProbe::ParameterIndicator,SQLLEN>);
    SQLHENV environment=SQL_NULL_HENV;
    assert(SQLAllocHandle(SQL_HANDLE_ENV,SQL_NULL_HANDLE,&environment)==SQL_SUCCESS);
    assert(SQLSetEnvAttr(environment,SQL_ATTR_ODBC_VERSION,reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3),0)==SQL_SUCCESS);
    assert(SQLFreeHandle(SQL_HANDLE_ENV,environment)==SQL_SUCCESS);
    std::cout<<"PASS original ODBC declarations, 64-bit indicators and native driver-manager allocation\n";
}
