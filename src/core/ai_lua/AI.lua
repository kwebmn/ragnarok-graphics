-- uaRO Graphics AI 2 (3.0.11-beta.290, 3.0.19-beta.19): the homunculus' and the mercenary's own AI, for uaRO (Lua 5.0).
-- Turned on in the game by /hoai (the homunculus) and /merai (the mercenary). Its choices: uaro_config.lua, written by
-- uaRO Graphics from its settings («Інтерфейс» → «ШІ гомункула»). A file without this first line is never replaced.

UARO_AGGRESSIVE = 0   -- attacks monsters near the owner by itself
UARO_DEFEND = 1       -- attacks what attacks the owner or itself
UARO_SKILLS = 1       -- uses its skills (heals the owner, strikes)
UARO_FOLLOW = 3       -- cells from the owner it keeps
UARO_HEAL_AT = 50     -- the owner's HP per cent to heal under
UARO_SKILL_LEVEL = 1  -- the level its skills are used at
-- 3.0.19-beta.19 (the testers: «дуже дуже далеко побіжав», «я ухожу він має йти за мною»): it keeps near the owner
UARO_REACH = 5        -- cells from the owner it attacks monsters by itself within
UARO_LEASH = 7        -- cells from the owner it fights within; past them back to it (an ordered attack aside)
UARO_STRIKE_AT = 50   -- its own HP per cent to strike with its skill under (Filir, Vanilmirth): blows while it is well
pcall(function() dofile("./AI/USER_AI/uaro_config.lua") end)

V_OWNER, V_POSITION, V_TYPE, V_MOTION, V_ATTACKRANGE, V_TARGET = 0, 1, 2, 3, 4, 5
V_SKILLATTACKRANGE, V_HOMUNTYPE, V_HP, V_SP, V_MAXHP, V_MAXSP = 6, 7, 8, 9, 10, 11
MOTION_MOVE, MOTION_DEAD = 1, 3
NONE_CMD, MOVE_CMD, STOP_CMD, ATTACK_OBJECT_CMD, ATTACK_AREA_CMD = 0, 1, 2, 3, 4
PATROL_CMD, HOLD_CMD, SKILL_OBJECT_CMD, SKILL_AREA_CMD, FOLLOW_CMD = 5, 6, 7, 8, 9

local IDLE, CHASE, FIGHT, GOTO, STAY = 0, 1, 2, 3, 4
local state, target, goX, goY, healAt, goAt = IDLE, 0, 0, 0, 0, 0
local ordered, strikeAt = false, nil  -- the owner's own attack order; its skill's last strike

local function cells(a, b)
    local ax, ay = GetV(V_POSITION, a)
    local bx, by = GetV(V_POSITION, b)
    if ax == -1 or bx == -1 then return 99 end
    return math.max(math.abs(ax - bx), math.abs(ay - by))
end

local function alive(id)
    if id == 0 then return false end
    local x = GetV(V_POSITION, id)
    return x ~= -1 and GetV(V_MOTION, id) ~= MOTION_DEAD
end

local function hp(id)
    local most = GetV(V_MAXHP, id)
    if most == nil or most <= 0 then return 100 end
    return GetV(V_HP, id) * 100 / most
end

-- The homunculus' skills by its kind (1-4 and 9-12 Lif Amistr Filir Vanilmirth; 5-8, 13-16 the same evolved)
local function kind(myid)
    if UARO_MERC == 1 then return 0 end
    local t = GetV(V_HOMUNTYPE, myid)
    if t == nil then return 0 end
    while t > 4 do t = t - 4 end
    return t
end

local function heal(myid, owner)
    if UARO_SKILLS ~= 1 or kind(myid) ~= 1 then return false end
    if hp(owner) >= UARO_HEAL_AT or GetTick() - healAt < 3000 then return false end
    healAt = GetTick()
    SkillObject(myid, UARO_SKILL_LEVEL, 8002, owner)  -- Healing Hands
    return true
end

local function moving(id)
    return GetV(V_MOTION, id) == MOTION_MOVE
end

local function onUs(a, myid, owner)  -- the monster attacking the owner or it
    local on = GetV(V_TARGET, a)
    return on == owner or on == myid
end

-- Its skill under UARO_STRIKE_AT of its own HP, once in two seconds at most
local function strike(myid, t)
    if UARO_SKILLS ~= 1 or hp(myid) >= UARO_STRIKE_AT or GetV(V_SP, myid) < 30 then return end
    if strikeAt ~= nil and GetTick() - strikeAt < 2000 then return end
    local k = kind(myid)
    if k == 4 then SkillObject(myid, UARO_SKILL_LEVEL, 8013, t)  -- Caprice
    elseif k == 3 then SkillObject(myid, UARO_SKILL_LEVEL, 8009, t)  -- Moonlight
    else return end
    strikeAt = GetTick()
end

-- A monster attacking the owner or itself within UARO_LEASH (defend), else the nearest within UARO_REACH of the
-- owner standing (aggressive; none while the owner walks); 0 none
local function enemy(myid, owner)
    local best, bestCells = 0, 99
    local actors = GetActors()
    for i = 1, table.getn(actors) do
        local a = actors[i]
        if IsMonster(a) == 1 and alive(a) then
            local d = cells(a, owner)
            if UARO_DEFEND == 1 and d <= UARO_LEASH and onUs(a, myid, owner) and d < bestCells then
                best, bestCells = a, d
            end
        end
    end
    if best ~= 0 or UARO_AGGRESSIVE ~= 1 or moving(owner) then return best end
    for i = 1, table.getn(actors) do
        local a = actors[i]
        if IsMonster(a) == 1 and alive(a) then
            local d = cells(a, owner)
            if d <= UARO_REACH and d < bestCells then best, bestCells = a, d end
        end
    end
    return best
end

-- The fight given up: the monster gone or dead; one of its own led past UARO_LEASH from the owner, or the owner
-- walking off from it; an ordered one only past 14
local function letGo(myid, owner)
    if not alive(target) then return true end
    if ordered then return cells(target, owner) > 14 end
    local away = cells(myid, owner)
    if cells(target, owner) > UARO_LEASH or away > UARO_LEASH then return true end
    return moving(owner) and away > UARO_FOLLOW and not onUs(target, myid, owner)
end

local function command(myid, msg)
    local c = msg[1]
    if c == MOVE_CMD then
        state, goX, goY, goAt, ordered = GOTO, msg[2], msg[3], GetTick(), false
        Move(myid, goX, goY)
    elseif c == ATTACK_OBJECT_CMD then
        state, target, ordered = CHASE, msg[2], true
    elseif c == STOP_CMD or c == HOLD_CMD then
        state, target, ordered = STAY, 0, false
    elseif c == FOLLOW_CMD then
        ordered = false
        if state == STAY then state = IDLE else state, target = STAY, 0 end
    elseif c == SKILL_OBJECT_CMD then
        SkillObject(myid, msg[2], msg[3], msg[4])
    elseif c == SKILL_AREA_CMD then
        SkillGround(myid, msg[2], msg[3], msg[4], msg[5])
    end
end

function AI(myid)
    local owner = GetV(V_OWNER, myid)
    local msg = GetMsg(myid)
    if msg[1] == NONE_CMD then msg = GetResMsg(myid) end
    if msg[1] ~= NONE_CMD then command(myid, msg) end
    if heal(myid, owner) then return end
    if state == GOTO then
        local x, y = GetV(V_POSITION, myid)
        if (x == goX and y == goY) or GetTick() - goAt > 15000 then state = IDLE end
        return
    end
    if state == STAY then return end
    if state == IDLE then
        if cells(myid, owner) > UARO_FOLLOW then MoveToOwner(myid) return end
        local e = enemy(myid, owner)
        if e ~= 0 then state, target = CHASE, e end
    end
    if state == CHASE or state == FIGHT then
        if letGo(myid, owner) then
            state, target, ordered = IDLE, 0, false
            MoveToOwner(myid)
            return
        end
        local range = GetV(V_ATTACKRANGE, myid)
        if cells(myid, target) <= range then
            strike(myid, target)
            state = FIGHT
            Attack(myid, target)
        else
            state = CHASE
            local x, y = GetV(V_POSITION, target)
            Move(myid, x, y)
        end
    end
end
