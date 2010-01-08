#include "vartable.h"

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#include <boost/test/auto_unit_test.hpp>

BOOST_AUTO_TEST_CASE(VarTable_test)
{
    VarTable table;
    VarSpec Ci(VarSpec::Color, 1, ustring("Ci"));
    VarSpec s(VarSpec::Float, 1, ustring("s"));
    VarSpec t(VarSpec::Float, 1, ustring("t"));
    VarSpec st(VarSpec::Float, 2, ustring("st"));
    VarId Ci_id = table.getId(Ci);
    VarId s_id = table.getId(s);
    VarId t_id = table.getId(t);
    VarId st_id = table.getId(st);

    BOOST_CHECK_EQUAL(table.getVar(Ci_id), Ci);
    BOOST_CHECK_EQUAL(table.getVar(s_id), s);
    BOOST_CHECK_EQUAL(table.getVar(t_id), t);
    BOOST_CHECK_EQUAL(table.getVar(st_id), st);

    BOOST_CHECK_EQUAL(table.getId(Ci), Ci_id);
}
