# Adicionais Excellent por qualidade

Cada adicional tem seu proprio sorteio C/B/A/S. A qualidade do item nao determina a qualidade dos adicionais.

| Adicional | C — Comum | B — Raro | A — Lendario | S — Epico | Secao no MainInfo |
|---|---:|---:|---:|---:|---|
| Chance Excellent | +6% | +8% | +9% | +10% | ExcellentDamageRate |
| Dano por nivel | nivel/30 | nivel/25 | nivel/20 | nivel/15 | ExcellentDamagePerLevel |
| Aumento de dano | +6% | +8% | +9% | +10% | ExcellentDamagePercent |
| Velocidade | +20 | +30 | +35 | +40 | ExcellentAttackSpeed |
| Vida recuperada por kill | 1/12 | 1/10 | 1/8 | 1/6 | ExcellentLifeOnKill |
| Mana recuperada por kill | 1/12 | 1/10 | 1/8 | 1/6 | ExcellentManaOnKill |
| SD maximo | +2% | +3% | +4% | +5% | ExcellentMaximumSD |
| Reducao de dano | +2% | +3% | +4% | +5% | ExcellentDamageReduction |
| Reflexao de dano | +2% | +3% | +4% | +5% | ExcellentReflect |
| Taxa de defesa PvM | +6% | +9% | +12% | +15% | ExcellentDefenseRatePvM |
| Zen obtido | +10% | +15% | +20% | +25% | ExcellentZenRate |
| Vida maxima | +1% | +2% | +3% | +4% | LifePercent |

A referencia MuDream usa tres ranks. O NeonMU conserva quatro, acrescentando C abaixo dos demais e mantendo a vida definida pelo usuario. Fonte: https://mudream.online/pt/about/options/excellent

Cores: C branco; B azul; A laranja/dourado; S violeta. O tooltip exibe nomes de qualidade e valores efetivos enviados pelo servidor.

A troca de mana maxima por SD maximo se aplica aos adicionais Excellent com tier salvo. Equipamentos sem metadados preservam as regras antigas. Asas e bonus de conjunto Ancient conservam seus efeitos especificos. O adicional individual Ancient continua usando AdditionalTierPercent.

A recuperacao por kill e acumulada como fracao; nao e arredondada previamente para porcentagem inteira. Ex.: 1/6 de 1000 recupera 166, somando separadamente a recuperacao normal do jogo.

Apos alterar o MainInfo: execute o Encoder e copie os sidecars para o servidor/cliente. Reinicie os GameServers e o cliente para recarregar configuracoes em cache. As letras salvas nao sao sorteadas de novo.
