#include <gtest/gtest.h>
#include "utils.h" 

TEST(utils, bisect) {
    auto r = utils::bisect("", '?');
    EXPECT_STREQ(r.first.c_str(), "");
    EXPECT_STREQ(r.second.c_str(), "");

    r = utils::bisect(":", ':');
    EXPECT_STREQ(r.first.c_str(), "");
    EXPECT_STREQ(r.second.c_str(), "");

    r = utils::bisect("abc=", '=');
    EXPECT_STREQ(r.first.c_str(), "abc");
    EXPECT_STREQ(r.second.c_str(), "");

    r = utils::bisect("abc=", '$');
    EXPECT_STREQ(r.first.c_str(), "abc=");
    EXPECT_STREQ(r.second.c_str(), "");

    r = utils::bisect("abc=def", '=');
    EXPECT_STREQ(r.first.c_str(), "abc");
    EXPECT_STREQ(r.second.c_str(), "def");

    r = utils::bisect("abc=def=ghi", '=');
    EXPECT_STREQ(r.first.c_str(), "abc");
    EXPECT_STREQ(r.second.c_str(), "def=ghi");
}

TEST(utils, trim) {
    auto s = utils::trim("");
    EXPECT_STREQ(s.c_str(), "");

    s = utils::trim(" \n\t\v\f\r ");
    EXPECT_STREQ(s.c_str(), "");

    s = utils::trim(" \n\ttest string\v\f\r ");
    EXPECT_STREQ(s.c_str(), "test string");

    s = utils::trim(" \n\ttest");
    EXPECT_STREQ(s.c_str(), "test");

    s = utils::trim("test \n\t ");
    EXPECT_STREQ(s.c_str(), "test");
}

TEST(utils, split) {
    auto vec = utils::split("", 'x');
    EXPECT_EQ(vec.size(), 0);

    vec = utils::split("a", 'x');
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec[0].first, 0);
    EXPECT_EQ(vec[0].second, 1);

    vec = utils::split("abc", 'a');
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0].first, 0);
    EXPECT_EQ(vec[0].second, 0);
    EXPECT_EQ(vec[1].first, 1);
    EXPECT_EQ(vec[1].second, 2);

    vec = utils::split("abc", 'c');
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0].first, 0);
    EXPECT_EQ(vec[0].second, 2);
    EXPECT_EQ(vec[1].first, 3);
    EXPECT_EQ(vec[1].second, 0);

    vec = utils::split(",ab,,c,,", ',');
    EXPECT_EQ(vec.size(), 6);
    EXPECT_EQ(vec[0].first, 0);
    EXPECT_EQ(vec[0].second, 0);
    EXPECT_EQ(vec[1].first, 1);
    EXPECT_EQ(vec[1].second, 2);
    EXPECT_EQ(vec[2].first, 4);
    EXPECT_EQ(vec[2].second, 0);
    EXPECT_EQ(vec[3].first, 5);
    EXPECT_EQ(vec[3].second, 1);
    EXPECT_EQ(vec[4].first, 7);
    EXPECT_EQ(vec[4].second, 0);
    EXPECT_EQ(vec[5].first, 8);
    EXPECT_EQ(vec[5].second, 0);

    vec = utils::split(",,", ',');
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0].first, 0);
    EXPECT_EQ(vec[0].second, 0);
    EXPECT_EQ(vec[1].first, 1);
    EXPECT_EQ(vec[1].second, 0);
    EXPECT_EQ(vec[2].first, 2);
    EXPECT_EQ(vec[2].second, 0);

    vec = utils::split(",", ',');
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0].first, 0);
    EXPECT_EQ(vec[0].second, 0);
    EXPECT_EQ(vec[1].first, 1);
    EXPECT_EQ(vec[1].second, 0);

    vec = utils::split("a,bb,ccc", ',');
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0].first, 0);
    EXPECT_EQ(vec[0].second, 1);
    EXPECT_EQ(vec[1].first, 2);
    EXPECT_EQ(vec[1].second, 2);
    EXPECT_EQ(vec[2].first, 5);
    EXPECT_EQ(vec[2].second, 3);
}