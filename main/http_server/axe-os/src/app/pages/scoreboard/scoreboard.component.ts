import { Component, OnDestroy, OnInit } from '@angular/core';
import { Subscription, catchError, of, switchMap, timer } from 'rxjs';
import { SystemService } from '../../services/system.service';
import { IScoreboardEntry } from '../../models/IScoreboardV2';

const REFRESH_MS = 30_000;

@Component({
  selector: 'app-scoreboard',
  templateUrl: './scoreboard.component.html',
  styleUrls: ['./scoreboard.component.scss']
})
export class ScoreboardComponent implements OnInit, OnDestroy {

  public entries: IScoreboardEntry[] | null = null;
  public loadError = false;
  public nowSec = 0; // refreshed with each fetch, for the "x ago" column

  private sub?: Subscription;

  constructor(private systemService: SystemService) { }

  ngOnInit(): void {
    this.sub = timer(0, REFRESH_MS).pipe(
      switchMap(() => this.systemService.getScoreboardV2().pipe(
        catchError(() => {
          this.loadError = true;
          return of(null);
        })
      ))
    ).subscribe(data => {
      if (data) {
        this.entries = data.entries ?? [];
        this.nowSec = Math.floor(Date.now() / 1000);
        this.loadError = false;
      }
    });
  }

  ngOnDestroy(): void {
    this.sub?.unsubscribe();
  }

  // The firmware sends seconds; Angular's date pipe wants milliseconds.
  public toMs(ts: number): number {
    return ts * 1000;
  }
}
