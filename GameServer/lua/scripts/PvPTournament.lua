-- Torneio PvP mata-mata 1x1
-- Ajuste somente esta area de configuracao.
local Config = {
    -- Class 239 usa o modelo Tournament que ja existe neste Main.
    -- Stadium/Arena de eventos (mapa 6).
    -- Outros pontos: VIP 122,21 | Pega-Pega 36,144 | Sobrevivente 97,149 |
    -- Especiais 128,139 | 4 lutas 91,227 | Rei Killer 127,185.
    Npc = { Class = 239, Map = 6, X = 160, Y = 140, Dir = 3 },

    -- Mata-mata: espera em 160,142 e duelo ao redor do centro 155,147.
    Wait = { Map = 6, X = 160, Y = 142, Dir = 3 },
    Arena = { Map = 6, XA = 153, YA = 147, XB = 157, YB = 147, Dir = 3 },
    Winner = { Map = 6, X = 160, Y = 142, Dir = 3 },
    Loser = { Map = 6, X = 160, Y = 142, Dir = 3 },

    -- Limite fechado do ringue. Somente os dois sorteados permanecem dentro.
    Ring = { Enabled = true, Map = 6, MinX = 150, MinY = 144, MaxX = 160, MaxY = 151 },

    -- Day: -1 todos os dias, 0 domingo, 1 segunda ... 6 sabado
    Schedule = { Enabled = false, Day = -1, Hour = 20, Minute = 0, RegistrationMinutes = 10 },

    -- BestOf deve ser impar: 1, 3, 5... (3 = primeiro a vencer 2 rounds)
    BestOf = 3,

    -- Tipos: "coin", "vip" ou "item"
    Reward = {
        Type = "coin",
        CoinType = 1, CoinAmount = 1000,
        VipLevel = 1, VipDays = 7,
        ItemSection = 14, ItemType = 13, ItemLevel = 0
    }
}

SetPvPNpc(Config.Npc.Class)
SpawnPvPNpc(Config.Npc.Class, Config.Npc.Map, Config.Npc.X, Config.Npc.Y, Config.Npc.Dir)
SetPvPWaitGate(Config.Wait.Map, Config.Wait.X, Config.Wait.Y, Config.Wait.Dir)
SetPvPArena(Config.Arena.Map, Config.Arena.XA, Config.Arena.YA, Config.Arena.XB, Config.Arena.YB, Config.Arena.Dir)
SetPvPLoserGate(Config.Loser.Map, Config.Loser.X, Config.Loser.Y, Config.Loser.Dir)
SetPvPWinnerGate(Config.Winner.Map, Config.Winner.X, Config.Winner.Y, Config.Winner.Dir)
SetPvPRing(Config.Ring.Map, Config.Ring.MinX, Config.Ring.MinY, Config.Ring.MaxX, Config.Ring.MaxY, Config.Ring.Enabled)

if Config.Schedule.Enabled then
    SetPvPSchedule(Config.Schedule.Day, Config.Schedule.Hour, Config.Schedule.Minute,
        Config.Schedule.RegistrationMinutes, Config.BestOf)
else
    DisablePvPSchedule()
end

function OnPvPChampion(playerIndex)
    if Config.Reward.Type == "coin" then
        GivePvPCoin(playerIndex, Config.Reward.CoinType, Config.Reward.CoinAmount)
    elseif Config.Reward.Type == "vip" then
        GivePvPVip(playerIndex, Config.Reward.VipLevel, Config.Reward.VipDays)
    elseif Config.Reward.Type == "item" then
        GivePvPItem(playerIndex, Config.Reward.ItemSection, Config.Reward.ItemType, Config.Reward.ItemLevel)
    else
        Log("PvPTournament: tipo de premio invalido: " .. tostring(Config.Reward.Type))
        return
    end

    Broadcast("O campeao do Torneio PvP recebeu a premiacao!")
end
