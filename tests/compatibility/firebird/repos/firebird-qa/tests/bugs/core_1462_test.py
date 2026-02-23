#coding:utf-8

"""
ID:          issue-773
ISSUE:       773
TITLE:       Server crash caused by a buffer overrun in the optimizer when more than 255 relation references exist in the query
DESCRIPTION:
JIRA:        CORE-1462
FBTEST:      bugs.core_1462
"""

import pytest
from firebird.qa import *

init_script = """create table test (i integer);
create index test_i on test(i);
commit;
"""

db = db_factory(init=init_script)

test_script = """select test.i from test
   inner join test as t1 on t1.i = test.i
   inner join test as t2 on t2.i = test.i
   inner join test as t3 on t3.i = test.i
   inner join test as t4 on t4.i = test.i
   inner join test as t5 on t5.i = test.i
   inner join test as t6 on t6.i = test.i
   inner join test as t7 on t7.i = test.i
   inner join test as t8 on t8.i = test.i
   inner join test as t9 on t9.i = test.i
   inner join test as t10 on t10.i = test.i
   inner join test as t11 on t11.i = test.i
   inner join test as t12 on t12.i = test.i
   inner join test as t13 on t13.i = test.i
   inner join test as t14 on t14.i = test.i
   inner join test as t15 on t15.i = test.i
   inner join test as t16 on t16.i = test.i
   inner join test as t17 on t17.i = test.i
   inner join test as t18 on t18.i = test.i
   inner join test as t19 on t19.i = test.i
   inner join test as t20 on t20.i = test.i
   inner join test as t21 on t21.i = test.i
   inner join test as t22 on t22.i = test.i
   inner join test as t23 on t23.i = test.i
   inner join test as t24 on t24.i = test.i
   inner join test as t25 on t25.i = test.i
   inner join test as t26 on t26.i = test.i
   inner join test as t27 on t27.i = test.i
   inner join test as t28 on t28.i = test.i
   inner join test as t29 on t29.i = test.i
   inner join test as t30 on t30.i = test.i
   inner join test as t31 on t31.i = test.i
   inner join test as t32 on t32.i = test.i
   inner join test as t33 on t33.i = test.i
   inner join test as t34 on t34.i = test.i
   inner join test as t35 on t35.i = test.i
   inner join test as t36 on t36.i = test.i
   inner join test as t37 on t37.i = test.i
   inner join test as t38 on t38.i = test.i
   inner join test as t39 on t39.i = test.i
   inner join test as t40 on t40.i = test.i
   inner join test as t41 on t41.i = test.i
   inner join test as t42 on t42.i = test.i
   inner join test as t43 on t43.i = test.i
   inner join test as t44 on t44.i = test.i
   inner join test as t45 on t45.i = test.i
   inner join test as t46 on t46.i = test.i
   inner join test as t47 on t47.i = test.i
   inner join test as t48 on t48.i = test.i
   inner join test as t49 on t49.i = test.i
   inner join test as t50 on t50.i = test.i
   inner join test as t51 on t51.i = test.i
   inner join test as t52 on t52.i = test.i
   inner join test as t53 on t53.i = test.i
   inner join test as t54 on t54.i = test.i
   inner join test as t55 on t55.i = test.i
   inner join test as t56 on t56.i = test.i
   inner join test as t57 on t57.i = test.i
   inner join test as t58 on t58.i = test.i
   inner join test as t59 on t59.i = test.i
   inner join test as t60 on t60.i = test.i
   inner join test as t61 on t61.i = test.i
   inner join test as t62 on t62.i = test.i
   inner join test as t63 on t63.i = test.i
   inner join test as t64 on t64.i = test.i
   inner join test as t65 on t65.i = test.i
   inner join test as t66 on t66.i = test.i
   inner join test as t67 on t67.i = test.i
   inner join test as t68 on t68.i = test.i
   inner join test as t69 on t69.i = test.i
   inner join test as t70 on t70.i = test.i
   inner join test as t71 on t71.i = test.i
   inner join test as t72 on t72.i = test.i
   inner join test as t73 on t73.i = test.i
   inner join test as t74 on t74.i = test.i
   inner join test as t75 on t75.i = test.i
   inner join test as t76 on t76.i = test.i
   inner join test as t77 on t77.i = test.i
   inner join test as t78 on t78.i = test.i
   inner join test as t79 on t79.i = test.i
   inner join test as t80 on t80.i = test.i
   inner join test as t81 on t81.i = test.i
   inner join test as t82 on t82.i = test.i
   inner join test as t83 on t83.i = test.i
   inner join test as t84 on t84.i = test.i
   inner join test as t85 on t85.i = test.i
   inner join test as t86 on t86.i = test.i
   inner join test as t87 on t87.i = test.i
   inner join test as t88 on t88.i = test.i
   inner join test as t89 on t89.i = test.i
   inner join test as t90 on t90.i = test.i
   inner join test as t91 on t91.i = test.i
   inner join test as t92 on t92.i = test.i
   inner join test as t93 on t93.i = test.i
   inner join test as t94 on t94.i = test.i
   inner join test as t95 on t95.i = test.i
   inner join test as t96 on t96.i = test.i
   inner join test as t97 on t97.i = test.i
   inner join test as t98 on t98.i = test.i
   inner join test as t99 on t99.i = test.i
   inner join test as t100 on t100.i = test.i
   inner join test as t101 on t101.i = test.i
   inner join test as t102 on t102.i = test.i
   inner join test as t103 on t103.i = test.i
   inner join test as t104 on t104.i = test.i
   inner join test as t105 on t105.i = test.i
   inner join test as t106 on t106.i = test.i
   inner join test as t107 on t107.i = test.i
   inner join test as t108 on t108.i = test.i
   inner join test as t109 on t109.i = test.i
   inner join test as t110 on t110.i = test.i
   inner join test as t111 on t111.i = test.i
   inner join test as t112 on t112.i = test.i
   inner join test as t113 on t113.i = test.i
   inner join test as t114 on t114.i = test.i
   inner join test as t115 on t115.i = test.i
   inner join test as t116 on t116.i = test.i
   inner join test as t117 on t117.i = test.i
   inner join test as t118 on t118.i = test.i
   inner join test as t119 on t119.i = test.i
   inner join test as t120 on t120.i = test.i
   inner join test as t121 on t121.i = test.i
   inner join test as t122 on t122.i = test.i
   inner join test as t123 on t123.i = test.i
   inner join test as t124 on t124.i = test.i
   inner join test as t125 on t125.i = test.i
   inner join test as t126 on t126.i = test.i
   inner join test as t127 on t127.i = test.i
   inner join test as t128 on t128.i = test.i
   inner join test as t129 on t129.i = test.i
   inner join test as t130 on t130.i = test.i
   inner join test as t131 on t131.i = test.i
   inner join test as t132 on t132.i = test.i
   inner join test as t133 on t133.i = test.i
   inner join test as t134 on t134.i = test.i
   inner join test as t135 on t135.i = test.i
   inner join test as t136 on t136.i = test.i
   inner join test as t137 on t137.i = test.i
   inner join test as t138 on t138.i = test.i
   inner join test as t139 on t139.i = test.i
   inner join test as t140 on t140.i = test.i
   inner join test as t141 on t141.i = test.i
   inner join test as t142 on t142.i = test.i
   inner join test as t143 on t143.i = test.i
   inner join test as t144 on t144.i = test.i
   inner join test as t145 on t145.i = test.i
   inner join test as t146 on t146.i = test.i
   inner join test as t147 on t147.i = test.i
   inner join test as t148 on t148.i = test.i
   inner join test as t149 on t149.i = test.i
   inner join test as t150 on t150.i = test.i
   inner join test as t151 on t151.i = test.i
   inner join test as t152 on t152.i = test.i
   inner join test as t153 on t153.i = test.i
   inner join test as t154 on t154.i = test.i
   inner join test as t155 on t155.i = test.i
   inner join test as t156 on t156.i = test.i
   inner join test as t157 on t157.i = test.i
   inner join test as t158 on t158.i = test.i
   inner join test as t159 on t159.i = test.i
   inner join test as t160 on t160.i = test.i
   inner join test as t161 on t161.i = test.i
   inner join test as t162 on t162.i = test.i
   inner join test as t163 on t163.i = test.i
   inner join test as t164 on t164.i = test.i
   inner join test as t165 on t165.i = test.i
   inner join test as t166 on t166.i = test.i
   inner join test as t167 on t167.i = test.i
   inner join test as t168 on t168.i = test.i
   inner join test as t169 on t169.i = test.i
   inner join test as t170 on t170.i = test.i
   inner join test as t171 on t171.i = test.i
   inner join test as t172 on t172.i = test.i
   inner join test as t173 on t173.i = test.i
   inner join test as t174 on t174.i = test.i
   inner join test as t175 on t175.i = test.i
   inner join test as t176 on t176.i = test.i
   inner join test as t177 on t177.i = test.i
   inner join test as t178 on t178.i = test.i
   inner join test as t179 on t179.i = test.i
   inner join test as t180 on t180.i = test.i
   inner join test as t181 on t181.i = test.i
   inner join test as t182 on t182.i = test.i
   inner join test as t183 on t183.i = test.i
   inner join test as t184 on t184.i = test.i
   inner join test as t185 on t185.i = test.i
   inner join test as t186 on t186.i = test.i
   inner join test as t187 on t187.i = test.i
   inner join test as t188 on t188.i = test.i
   inner join test as t189 on t189.i = test.i
   inner join test as t190 on t190.i = test.i
   inner join test as t191 on t191.i = test.i
   inner join test as t192 on t192.i = test.i
   inner join test as t193 on t193.i = test.i
   inner join test as t194 on t194.i = test.i
   inner join test as t195 on t195.i = test.i
   inner join test as t196 on t196.i = test.i
   inner join test as t197 on t197.i = test.i
   inner join test as t198 on t198.i = test.i
   inner join test as t199 on t199.i = test.i
   inner join test as t200 on t200.i = test.i
   inner join test as t201 on t201.i = test.i
   inner join test as t202 on t202.i = test.i
   inner join test as t203 on t203.i = test.i
   inner join test as t204 on t204.i = test.i
   inner join test as t205 on t205.i = test.i
   inner join test as t206 on t206.i = test.i
   inner join test as t207 on t207.i = test.i
   inner join test as t208 on t208.i = test.i
   inner join test as t209 on t209.i = test.i
   inner join test as t210 on t210.i = test.i
   inner join test as t211 on t211.i = test.i
   inner join test as t212 on t212.i = test.i
   inner join test as t213 on t213.i = test.i
   inner join test as t214 on t214.i = test.i
   inner join test as t215 on t215.i = test.i
   inner join test as t216 on t216.i = test.i
   inner join test as t217 on t217.i = test.i
   inner join test as t218 on t218.i = test.i
   inner join test as t219 on t219.i = test.i
   inner join test as t220 on t220.i = test.i
   inner join test as t221 on t221.i = test.i
   inner join test as t222 on t222.i = test.i
   inner join test as t223 on t223.i = test.i
   inner join test as t224 on t224.i = test.i
   inner join test as t225 on t225.i = test.i
   inner join test as t226 on t226.i = test.i
   inner join test as t227 on t227.i = test.i
   inner join test as t228 on t228.i = test.i
   inner join test as t229 on t229.i = test.i
   inner join test as t230 on t230.i = test.i
   inner join test as t231 on t231.i = test.i
   inner join test as t232 on t232.i = test.i
   inner join test as t233 on t233.i = test.i
   inner join test as t234 on t234.i = test.i
   inner join test as t235 on t235.i = test.i
   inner join test as t236 on t236.i = test.i
   inner join test as t237 on t237.i = test.i
   inner join test as t238 on t238.i = test.i
   inner join test as t239 on t239.i = test.i
   inner join test as t240 on t240.i = test.i
   inner join test as t241 on t241.i = test.i
   inner join test as t242 on t242.i = test.i
   inner join test as t243 on t243.i = test.i
   inner join test as t244 on t244.i = test.i
   inner join test as t245 on t245.i = test.i
   inner join test as t246 on t246.i = test.i
   inner join test as t247 on t247.i = test.i
   inner join test as t248 on t248.i = test.i
   inner join test as t249 on t249.i = test.i
   inner join test as t250 on t250.i = test.i
   inner join test as t251 on t251.i = test.i
   inner join test as t252 on t252.i = test.i
   inner join test as t253 on t253.i = test.i
   inner join test as t254 on t254.i = test.i
   inner join test as t255 on t255.i = test.i
   inner join test as t256 on t256.i = test.i
   ;
"""

act = isql_act('db', test_script)

expected_stderr = """Statement failed, SQLSTATE = 54001
Dynamic SQL Error
-Too many Contexts of Relation/Procedure/Views. Maximum allowed is 256
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

