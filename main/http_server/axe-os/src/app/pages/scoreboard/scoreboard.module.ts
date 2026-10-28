import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { NbCardModule } from '@nebular/theme';
import { TranslateModule } from '@ngx-translate/core';
import { PipesModule } from '../../pipes/pipes.module';
import { ScoreboardComponent } from './scoreboard.component';

@NgModule({
  declarations: [
    ScoreboardComponent
  ],
  imports: [
    CommonModule,
    NbCardModule,
    TranslateModule,
    PipesModule,
  ],
  exports: [
    ScoreboardComponent
  ]
})
export class ScoreboardModule { }
