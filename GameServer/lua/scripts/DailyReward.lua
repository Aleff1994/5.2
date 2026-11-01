-- Premio Diario: configuracao recarregada automaticamente pelo GameServer.
-- os.date usa o fuso horario local da maquina do GameServer.

-- ================================================================
-- CONFIGURACAO DOS PREMIOS
-- ================================================================
-- O numero entre colchetes e o DIA DO MES:
--     [1]  = dia 1
--     [15] = dia 15
--     [31] = dia 31
--     [32] = bonus mensal (nao aparece como dia no calendario)
--
-- CAMPOS DE CADA PREMIO:
--     section = categoria do item no Item.txt
--     index   = numero do item dentro da categoria
--     level   = nivel do item (normalmente de 0 ate 15)
--     amount  = quantidade entregue ao jogador
--     name    = nome exibido no aviso ao resgatar
--
-- CODIGOS DE JOIAS USADOS NESTE EXEMPLO:
--     section=14, index=13 : Jewel of Bless
--     section=14, index=14 : Jewel of Soul
--     section=14, index=16 : Jewel of Life
--     section=14, index=22 : Jewel of Creation
--     section=14, index=31 : Jewel of Guardian
--     section=14, index=41 : Gemstone
--     section=14, index=42 : Jewel of Harmony
--
-- EXEMPLO DE EDICAO:
--     [1] = Item { section=14, index=13, level=0, amount=5, name="5 Jewel of Bless" },
-- ================================================================

local function Item(config)
    config.kind = 0 -- 0 significa recompensa em item do jogo
    return config
end

local Rewards = {
    -- DIA 01: arma +0
    [1]  = Item { section=0,  index=1,   level=0,  amount=1, name="Short Sword +0" },
    -- DIA 02: arma +1
    [2]  = Item { section=0,  index=0,   level=1,  amount=1, name="Kris +1" },
    -- DIA 03: arma +2
    [3]  = Item { section=0,  index=3,   level=2,  amount=1, name="Katana +2" },
    -- DIA 04: arma +3
    [4]  = Item { section=0,  index=5,   level=3,  amount=1, name="Blade +3" },
    -- DIA 05: arma +4
    [5]  = Item { section=0,  index=15,  level=4,  amount=1, name="Giant Sword +4" },
    -- DIA 06: arma +5
    [6]  = Item { section=0,  index=16,  level=5,  amount=1, name="Sword of Destruction +5" },
    -- DIA 07: arma +6
    [7]  = Item { section=0,  index=17,  level=6,  amount=1, name="Dark Breaker +6" },
    -- DIA 08: arma +7
    [8]  = Item { section=2,  index=5,   level=7,  amount=1, name="Crystal Sword +7" },
    -- DIA 09: machado +8
    [9]  = Item { section=2,  index=6,   level=8,  amount=1, name="Chaos Dragon Axe +8" },
    -- DIA 10: arco +9
    [10] = Item { section=4,  index=17,  level=9,  amount=1, name="Celestial Bow +9" },
    -- DIA 11: cajado +10
    [11] = Item { section=5,  index=5,   level=10, amount=1, name="Legendary Staff +10" },
    -- DIA 12: cajado +11
    [12] = Item { section=5,  index=11,  level=11, amount=1, name="Staff of Kundun +11" },
    -- DIA 13: escudo +12
    [13] = Item { section=6,  index=13,  level=12, amount=1, name="Dragon Shield +12" },
    -- DIA 14: armadura +13
    [14] = Item { section=8,  index=21,  level=13, amount=1, name="Great Dragon Armor +13" },

    -- DIAS 15 A 19: conjunto Dragon Knight
    [15] = Item { section=7,  index=29,  level=0,  amount=1, name="Dragon Knight Helm +0" },
    [16] = Item { section=8,  index=29,  level=3,  amount=1, name="Dragon Knight Armor +3" },
    [17] = Item { section=9,  index=29,  level=6,  amount=1, name="Dragon Knight Pants +6" },
    [18] = Item { section=10, index=29,  level=9,  amount=1, name="Dragon Knight Gloves +9" },
    [19] = Item { section=11, index=29,  level=13, amount=1, name="Dragon Knight Boots +13" },

    -- DIAS 20 A 24: asas existentes no cliente
    [20] = Item { section=12, index=0,   level=0,  amount=1, name="Wings of Elf +0" },
    [21] = Item { section=12, index=1,   level=3,  amount=1, name="Wings of Heaven +3" },
    [22] = Item { section=12, index=2,   level=6,  amount=1, name="Wings of Satan +6" },
    [23] = Item { section=12, index=36,  level=9,  amount=1, name="Wing of Storm +9" },
    [24] = Item { section=12, index=37,  level=13, amount=1, name="Wing of Eternal +13" },

    -- DIAS 25 A 28: penas e materiais (+0 porque nao usam nivel)
    [25] = Item { section=13, index=14,  level=0,  amount=1, name="Loch's Feather" },
    [26] = Item { section=13, index=52,  level=0,  amount=1, name="Condor Flame" },
    [27] = Item { section=13, index=53,  level=0,  amount=1, name="Condor Feather" },
    [28] = Item { section=14, index=67,  level=0,  amount=1, name="Dark Phoenix Feather" },

    -- DIAS 29 A 31: itens especiais
    [29] = Item { section=12, index=15,  level=0,  amount=1, name="Jewel of Chaos" },
    [30] = Item { section=13, index=37,  level=0,  amount=1, name="Horn of Fenrir" },
    [31] = Item { section=12, index=200, level=13, amount=1, name="Wing of Maximos +13" },

    -- DIA 32: BONUS POR COMPLETAR TODOS OS DIAS DO MES
    [32] = Item { section=13, index=53,  level=0,  amount=1, name="Bonus mensal: Condor Feather" },
}
local function IsLeapYear(year)
    return (year % 400 == 0) or (year % 4 == 0 and year % 100 ~= 0)
end

local function DaysInMonth(year, month)
    local days = {31,28,31,30,31,30,31,31,30,31,30,31}
    if month == 2 and IsLeapYear(year) then return 29 end
    return days[month]
end

function GetFirstDayWeekday(year, month)
    local stamp = os.time({year=year, month=month, day=1, hour=12})
    return tonumber(os.date("%w", stamp)) + 1 -- 1=DOM ... 7=SAB
end

function GetDailyRewardCalendar()
    local now = os.date("*t")
    return {
        today = now.day,
        daysInMonth = DaysInMonth(now.year, now.month),
        startWeekday = GetFirstDayWeekday(now.year, now.month)
    }
end

function GetDailyRewardDefinition(day)
    return Rewards[day]
end

function ReloadDailyRewardConfig()
    Log("[DailyReward] configuracao recarregada")
end
