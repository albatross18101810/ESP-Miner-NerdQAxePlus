export interface IScoreboardEntry {
    rank: number;
    difficulty: number;
    timestamp: number; // unix seconds, 0 = clock was not synced when the share was found
}

export interface IScoreboardV2 {
    entries: IScoreboardEntry[];
}
