#!/usr/bin/env python3
"""test_ai_lua.py: the homunculus' and the mercenary's own AI (src/core/ai_lua/AI.lua) run on scripted worlds.

3.0.19-beta.19 (the testers' round 3: «дуже дуже далеко побіжав», «я ухожу він має йти за мною», «коли хп менше
половини начинае кастувати магію свою»). The game runs the AI with its Lua 5.0; here the host's Lua 5.2 library
(ctypes) runs it, table.getn given; 5.0's syntax (no # or % operators) checked on the text. No library: SKIP.
"""
import ctypes
import ctypes.util
import os
import re
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
AI = os.path.join(HERE, '..', 'src', 'core', 'ai_lua', 'AI.lua')


def load_lua():
    for name in ('liblua5.2.so.0', 'liblua5.2.so', ctypes.util.find_library('lua5.2')):
        if not name:
            continue
        try:
            return ctypes.CDLL(name)
        except OSError:
            continue
    return None


LUA = load_lua()

# The game's side: positions, motions, targets, HP and SP by id; the AI's calls written to W.out
PRELUDE = r'''
table.getn = function(t) return #t end
W = {tick = 1000, owner = 1, pos = {}, motion = {}, target = {}, hp = {}, maxhp = {}, sp = {}, homuntype = 4,
     range = 1, monsters = {}, msgs = {}, out = {}}
local function put(s) W.out[#W.out + 1] = s end
function GetV(v, id)
    if v == 0 then return W.owner end
    if v == 1 then local p = W.pos[id]; if p == nil then return -1, -1 end; return p[1], p[2] end
    if v == 2 then return 0 end
    if v == 3 then return W.motion[id] or 0 end
    if v == 4 then return W.range end
    if v == 5 then return W.target[id] or 0 end
    if v == 6 then return 1 end
    if v == 7 then return W.homuntype end
    if v == 8 then return W.hp[id] or 100 end
    if v == 9 then return W.sp[id] or 100 end
    if v == 10 then return W.maxhp[id] or 100 end
    if v == 11 then return 100 end
    return 0
end
function GetActors()
    local t = {}
    for id, _ in pairs(W.pos) do t[#t + 1] = id end
    table.sort(t)
    return t
end
function IsMonster(id) if W.monsters[id] then return 1 end return 0 end
function GetMsg(id) if #W.msgs > 0 then return table.remove(W.msgs, 1) end return {0} end
function GetResMsg(id) return {0} end
function GetTick() return W.tick end
function TraceAI(s) end
function Move(id, x, y) put('Move ' .. x .. ' ' .. y) end
function MoveToOwner(id) put('MoveToOwner') end
function Attack(id, t) put('Attack ' .. t) end
function SkillObject(id, lv, sk, t) put('Skill ' .. sk .. ' ' .. t) end
function SkillGround(id, lv, sk, x, y) put('SkillGround ' .. sk) end
'''


class Lua:
    def __init__(self):
        L = LUA.luaL_newstate()
        if not L:
            raise RuntimeError('no Lua state')
        self.L = ctypes.c_void_p(L)
        LUA.luaL_openlibs(self.L)
        self.run(PRELUDE)
        with open(AI, encoding='utf-8') as f:
            self.run(f.read())

    def run(self, code):
        b = code.encode('utf-8')
        if LUA.luaL_loadstring(self.L, b) != 0 or LUA.lua_pcallk(self.L, 0, 0, 0, 0, None) != 0:
            msg = LUA.lua_tolstring(self.L, -1, None)
            LUA.lua_settop(self.L, 0)
            raise AssertionError('Lua: ' + (msg.decode('utf-8', 'replace') if msg else '?'))

    def frame(self, setup='', ticks=1):
        """One or more of the game's AI calls after `setup` (Lua); the calls the AI made, in order."""
        self.run(setup + '\nW.out = {}')
        for _ in range(ticks):
            self.run('AI(2); W.tick = W.tick + 100')
        self.run('OUT = table.concat(W.out, ";")')
        LUA.lua_getglobal(self.L, b'OUT')
        s = LUA.lua_tolstring(self.L, -1, None)
        LUA.lua_settop(self.L, 0)
        return (s or b'').decode('utf-8')


if LUA:
    LUA.luaL_newstate.restype = ctypes.c_void_p
    LUA.luaL_loadstring.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    LUA.lua_pcallk.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                               ctypes.c_void_p]
    LUA.lua_tolstring.restype = ctypes.c_char_p
    LUA.lua_tolstring.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
    LUA.lua_settop.argtypes = [ctypes.c_void_p, ctypes.c_int]
    LUA.lua_getglobal.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    LUA.luaL_openlibs.argtypes = [ctypes.c_void_p]

# The owner (1) at 100,100, the homunculus (2) next to it
WORLD = 'W.pos[1] = {100, 100}; W.pos[2] = {101, 100}; '


class Lua50Syntax(unittest.TestCase):
    def test_no_operators_lua_50_lacks(self):
        with open(AI, encoding='utf-8') as f:
            lines = f.read().splitlines()
        for n, line in enumerate(lines, 1):
            code = re.sub(r'"[^"]*"', '""', line.split('--', 1)[0])
            self.assertNotIn('#', code, 'line %d: # is not Lua 5.0' % n)
            self.assertNotIn('%', code, 'line %d: %% is not Lua 5.0' % n)


@unittest.skipIf(LUA is None, 'no Lua 5.2 library on this host')
class Companion(unittest.TestCase):
    def test_follows_the_owner_when_far(self):
        lua = Lua()
        self.assertEqual(lua.frame('W.pos[1] = {100, 100}; W.pos[2] = {106, 100}'), 'MoveToOwner')

    def test_by_itself_only_monsters_close_to_the_owner(self):
        lua = Lua()
        far = WORLD + 'UARO_AGGRESSIVE = 1; W.pos[10] = {108, 100}; W.monsters[10] = true'
        out = lua.frame(far)
        self.assertNotIn('Move 108', out)  # 8 cells off: left alone
        self.assertNotIn('Attack 10', out)
        near = 'W.pos[10] = {104, 100}'
        self.assertIn('Move 104 100', Lua().frame(far + '; ' + near))  # 4 cells off: attacked

    def test_lets_go_of_a_fight_drifting_away_from_the_owner(self):
        lua = Lua()
        lua.frame(WORLD + 'UARO_AGGRESSIVE = 1; W.pos[10] = {104, 100}; W.monsters[10] = true')
        out = lua.frame('W.pos[10] = {109, 100}; W.pos[2] = {108, 100}')  # the monster led it 8 cells off
        self.assertEqual(out, 'MoveToOwner')

    def test_follows_the_owner_walking_away_from_a_fight_of_its_own(self):
        lua = Lua()
        lua.frame(WORLD + 'UARO_AGGRESSIVE = 1; W.pos[10] = {104, 100}; W.monsters[10] = true')
        lua.frame('W.pos[2] = {103, 100}')  # fighting next to the monster
        out = lua.frame('W.pos[1] = {98, 100}; W.motion[1] = 1')  # the owner walks off: 5 cells now
        self.assertEqual(out, 'MoveToOwner')

    def test_still_defends_the_walking_owner(self):
        lua = Lua()
        out = lua.frame(WORLD + 'W.motion[1] = 1; W.pos[10] = {102, 101}; W.monsters[10] = true; W.target[10] = 1')
        self.assertIn('Attack 10', out)

    def test_an_ordered_attack_goes_past_the_leash(self):
        lua = Lua()
        out = lua.frame(WORLD + 'W.pos[10] = {111, 100}; W.monsters[10] = true; W.msgs = {{3, 10}}')
        self.assertIn('Move 111 100', out)

    def test_skill_only_under_half_its_hp_and_not_every_call(self):
        lua = Lua()  # Vanilmirth (type 4), the monster in reach
        setup = WORLD + 'W.pos[10] = {102, 100}; W.monsters[10] = true; W.target[10] = 1'
        self.assertNotIn('Skill', lua.frame(setup))  # its HP full: blows only
        out = lua.frame('W.hp[2] = 40')
        self.assertIn('Skill 8013 10', out)
        self.assertNotIn('Skill', lua.frame(''))  # the next call (0.1 s on): not again
        self.assertIn('Skill 8013 10', lua.frame('W.tick = W.tick + 3000'))


if __name__ == '__main__':
    unittest.main(verbosity=1)
