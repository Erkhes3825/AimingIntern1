#pragma once

namespace TicTac {

	using namespace System;
	using namespace System::Windows::Forms;
	using namespace System::Drawing;
	using namespace System::Drawing::Drawing2D;

	// ゲームの状態
	enum class GameState { Title, Playing, Demo };

	// バフの種類を定義
	enum class BuffType { None, Invincible, DoubleScore, SuperMagnet };

	// 障害物の構造体
	value struct Entity {
		Rectangle rect;
		float startY;
		float range;
		float speed;
		float offset;
		bool isMoving;
	};

	// 🦟 ハエ（ボーナスアイテム）の構造体
	value struct Bug {
		Rectangle rect;
		float baseY;
		bool isActive;
		bool isSpecial;
	};

	public ref class MyForm : System::Windows::Forms::Form
	{
	private:
		// --- 状態管理 ---
		GameState currentState = GameState::Title;
		int idleTime = 0;
		bool isGameStarted = false; // ゲーム開始待ちのフラグ

		Button^ btnRed;
		Button^ btnGreen;
		Button^ btnBlue;
		Color playerColor = Color::RoyalBlue;
		bool canShoot = true;
		int shootCooldown = 0; // 連打防止用のクールタイムカウンター
		const int MAX_COOLDOWN = 30; // クールタイムの最大値（0.5秒 = 30フレーム）

		// --- 💻 フルスクリーン・解像度スケーリング用変数 ---
		bool isFullscreen = false;
		float scaleX = 1.0f; // 描画・マウス座標のX倍率
		float scaleY = 1.0f; // 描画・マウス座標のY倍率

		// --- 物理・状態変数 ---
		float px, py, oldx, oldy;
		float anchorX, anchorY, ropeLength;
		bool isAttached = false;

		// --- 🔋 エネルギー（満腹度）変数 ---
		float currentEnergy = 420.0f;
		float MAX_ENERGY = 420.0f;

		float gravity = 0.35f;
		float damp = 0.97f;
		float springK = 0.15f;

		// --- 🌟 ランダムバフシステム変数 ---
		float buffTimer = 0.0f;
		BuffType currentBuff = BuffType::None;
		int nextSpecialScore = 250;

		// --- ステージ・スコア管理 ---
		array<Entity>^ worldObjects;
		array<Bug>^ bugs;
		Rectangle startPlatform;
		float cameraX = 0;
		float timeCount = 0;

		int score = 0;
		int highScore = 0;
		int bonusScore = 0;
		Random^ rnd;

		Timer^ gameTimer;

	public:
		MyForm(void) {
			InitializeComponent();
			this->DoubleBuffered = true;
			this->ClientSize = System::Drawing::Size(800, 600);

			rnd = gcnew Random();

			InitUI();
			InitLevel();
			ResetPlayer();

			gameTimer = gcnew Timer();
			gameTimer->Interval = 16;
			gameTimer->Tick += gcnew EventHandler(this, &MyForm::OnUpdate);
			gameTimer->Start();

			// イベントハンドラの追加
			this->MouseDown += gcnew MouseEventHandler(this, &MyForm::OnMouseDown);
			this->MouseUp += gcnew MouseEventHandler(this, &MyForm::OnMouseUp);
			this->MouseMove += gcnew MouseEventHandler(this, &MyForm::OnMouseMove);
			this->Paint += gcnew PaintEventHandler(this, &MyForm::OnPaint);

			// フルスクリーン切り替え[F11]と画面リサイズ用のイベントを追加
			this->KeyDown += gcnew KeyEventHandler(this, &MyForm::OnKeyDown);
			this->Resize += gcnew EventHandler(this, &MyForm::OnResize);
			this->KeyPreview = true; // フォーム全体でキー入力を最優先で受け取る設定
		}

	private:
		void InitUI() {
			System::Drawing::Font^ btnFont = gcnew System::Drawing::Font("Arial", 12, FontStyle::Bold);

			btnRed = gcnew Button();
			btnRed->Text = "RED";
			btnRed->BackColor = Color::Tomato;
			btnRed->ForeColor = Color::White;
			btnRed->Font = btnFont;
			btnRed->Click += gcnew EventHandler(this, &MyForm::OnBtnRedClick);
			this->Controls->Add(btnRed);

			btnGreen = gcnew Button();
			btnGreen->Text = "GREEN";
			btnGreen->BackColor = Color::MediumSeaGreen;
			btnGreen->ForeColor = Color::White;
			btnGreen->Font = btnFont;
			btnGreen->Click += gcnew EventHandler(this, &MyForm::OnBtnGreenClick);
			this->Controls->Add(btnGreen);

			btnBlue = gcnew Button();
			btnBlue->Text = "BLUE";
			btnBlue->BackColor = Color::RoyalBlue;
			btnBlue->ForeColor = Color::White;
			btnBlue->Font = btnFont;
			btnBlue->Click += gcnew EventHandler(this, &MyForm::OnBtnBlueClick);
			this->Controls->Add(btnBlue);

			// 初回のボタン位置を計算
			UpdateLayout();
		}

		// 画面サイズが変わったときにボタンの位置と大きさを自動追従させる関数
		void UpdateLayout() {
			if (btnRed == nullptr || btnGreen == nullptr || btnBlue == nullptr) return;

			float sX = (float)this->ClientSize.Width / 800.0f;
			float sY = (float)this->ClientSize.Height / 600.0f;

			btnRed->Location = Point((int)(220 * sX), (int)(480 * sY));
			btnRed->Size = System::Drawing::Size((int)(100 * sX), (int)(40 * sY));

			btnGreen->Location = Point((int)(350 * sX), (int)(480 * sY));
			btnGreen->Size = System::Drawing::Size((int)(100 * sX), (int)(40 * sY));

			btnBlue->Location = Point((int)(480 * sX), (int)(480 * sY));
			btnBlue->Size = System::Drawing::Size((int)(100 * sX), (int)(40 * sY));
		}

		void OnResize(Object^ sender, EventArgs^ e) {
			UpdateLayout();
		}

		// F11キーが押されたらフルスクリーンを切り替える
		void OnKeyDown(Object^ sender, KeyEventArgs^ e) {
			if (e->KeyCode == Keys::F11) {
				if (!isFullscreen) {
					// フルスクリーン化（枠を消して最大化）
					this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::None;
					this->WindowState = System::Windows::Forms::FormWindowState::Maximized;
					isFullscreen = true;
				}
				else {
					// 通常ウィンドウに戻す
					this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::Sizable;
					this->WindowState = System::Windows::Forms::FormWindowState::Normal;
					this->ClientSize = System::Drawing::Size(800, 600);
					isFullscreen = false;
				}
			}
		}

		void OnBtnRedClick(Object^ sender, EventArgs^ e) { playerColor = Color::Tomato; this->Invalidate(); }
		void OnBtnGreenClick(Object^ sender, EventArgs^ e) { playerColor = Color::MediumSeaGreen; this->Invalidate(); }
		void OnBtnBlueClick(Object^ sender, EventArgs^ e) { playerColor = Color::RoyalBlue; this->Invalidate(); }

		void DrawSpider(Graphics^ g, float x, float y, Color color) {
			SolidBrush^ brush = gcnew SolidBrush(color);
			Pen^ pen = gcnew Pen(color, 2);

			g->FillEllipse(brush, (int)(x - 12), (int)(y - 15), 24, 30);
			g->FillEllipse(brush, (int)(x - 8), (int)(y - 25), 16, 16);
			g->FillEllipse(Brushes::White, (int)(x - 6), (int)(y - 20), 5, 5);
			g->FillEllipse(Brushes::Black, (int)(x - 4), (int)(y - 18), 2, 2);
			g->FillEllipse(Brushes::White, (int)(x + 1), (int)(y - 20), 5, 5);
			g->FillEllipse(Brushes::Black, (int)(x + 2), (int)(y - 18), 2, 2);

			float animOffset = (isAttached) ? (float)Math::Sin(timeCount * 0.2f) * 3.0f : 0;
			for (int i = 0; i < 4; i++) {
				float rootY = y - 5.0f + i * 6.0f;
				float rootX_R = x + 8.0f;
				float rootX_L = x - 8.0f;
				float footY = y - 10.0f + i * 15.0f + animOffset;
				float footX_R = x + 30.0f + i * 3.0f;
				float footX_L = x - 30.0f - i * 3.0f;
				float kneeY = (rootY + footY) / 2.0f - 12.0f;
				float kneeX_R = (rootX_R + footX_R) / 2.0f + 8.0f;
				float kneeX_L = (rootX_L + footX_L) / 2.0f - 8.0f;

				g->DrawLine(pen, (int)rootX_R, (int)rootY, (int)kneeX_R, (int)kneeY);
				g->DrawLine(pen, (int)kneeX_R, (int)kneeY, (int)footX_R, (int)footY);
				g->DrawLine(pen, (int)rootX_L, (int)rootY, (int)kneeX_L, (int)kneeY);
				g->DrawLine(pen, (int)kneeX_L, (int)kneeY, (int)footX_L, (int)footY);
			}
		}

		void DrawCityBackground(Graphics^ g, float camX) {
			DrawBuildingLayer(g, camX, Color::FromArgb(40, 40, 50), 0.1f, 350, 100, 80, 1500);
			DrawBuildingLayer(g, camX, Color::FromArgb(60, 65, 80), 0.3f, 250, 80, 120, 2500);
		}

		void DrawBuildingLayer(Graphics^ g, float camX, Color color, float speedMult, int baseH, int varH, int width, int seedBase) {
			float effX = camX * speedMult;
			int spacing = width + 20;
			int startIdx = (int)(effX / spacing) - 1;
			int numToDraw = (this->ClientSize.Width / spacing) + 3;
			SolidBrush^ bBrush = gcnew SolidBrush(color);
			SolidBrush^ wBrush = gcnew SolidBrush(Color::FromArgb(100, 255, 255, 150));

			for (int i = startIdx; i < startIdx + numToDraw; i++) {
				Random^ bRnd = gcnew Random(seedBase + i * 555);
				int h = baseH + bRnd->Next(-varH, varH);
				int xPos = (int)(i * spacing - effX);
				g->FillRectangle(bBrush, xPos, 600 - h, width, h);

				if (bRnd->Next(0, 10) > 2 && h > 150) {
					for (int wy = 600 - h + 20; wy < 580; wy += 30) {
						for (int wx = xPos + 10; wx < xPos + width - 10; wx += 25) {
							if (bRnd->Next(0, 5) > 1) g->FillRectangle(wBrush, wx, wy, 10, 15);
						}
					}
				}
			}
		}

		void InitLevel() {
			startPlatform = Rectangle(50, 450, 200, 20);
			worldObjects = gcnew array<Entity>(5);
			for (int i = 0; i < worldObjects->Length; i++) {
				worldObjects[i].rect = Rectangle(600 + i * 400, 250, 60, 40);
				worldObjects[i].startY = (float)worldObjects[i].rect.Y;
				worldObjects[i].range = 100.0f;
				worldObjects[i].speed = 0.03f;
				worldObjects[i].offset = (float)i * 2.0f;
				worldObjects[i].isMoving = true;
			}

			bugs = gcnew array<Bug>(4);
			for (int i = 0; i < bugs->Length; i++) {
				bugs[i].rect = Rectangle(600 + i * 350, 200, 16, 16);
				bugs[i].baseY = 200.0f;
				bugs[i].isActive = true;
				bugs[i].isSpecial = false;
			}
		}

		void ResetPlayer() {
			px = startPlatform.X + 50;
			py = startPlatform.Y - 16;
			oldx = px;
			oldy = py;
			cameraX = 0;
			isAttached = false;
			isGameStarted = false; // リセット時は未開始状態に戻す
			currentEnergy = MAX_ENERGY;
			score = 0;
			bonusScore = 0;
			canShoot = true;
			shootCooldown = 0;
			buffTimer = 0.0f;
			currentBuff = BuffType::None;
			nextSpecialScore = 250;

			for (int i = 0; i < worldObjects->Length; i++) {
				worldObjects[i].rect.X = 600 + i * 400;
				worldObjects[i].startY = 250.0f;
				worldObjects[i].range = 100.0f;
				worldObjects[i].speed = 0.03f;
			}

			for (int i = 0; i < bugs->Length; i++) {
				bugs[i].rect.X = 600 + i * 350;
				bugs[i].baseY = (float)rnd->Next(100, 450);
				bugs[i].isActive = true;
				bugs[i].isSpecial = false;
			}
		}

		void GameOver() {
			gameTimer->Stop();

			if (currentState == GameState::Demo) {
				currentState = GameState::Title;
				btnRed->Visible = true;
				btnGreen->Visible = true;
				btnBlue->Visible = true;
				idleTime = 0;
				ResetPlayer();
				gameTimer->Start();
				return;
			}

			int finalScore = score + bonusScore;
			if (finalScore > highScore) highScore = finalScore;

			MessageBox::Show("GAME OVER!\n最終スコア: " + finalScore + " Pt", "Result");

			currentState = GameState::Title;

			btnRed->Visible = true;
			btnGreen->Visible = true;
			btnBlue->Visible = true;
			idleTime = 0;

			ResetPlayer();
			gameTimer->Start();
		}

		void OnUpdate(Object^ sender, EventArgs^ e) {
			timeCount += 1.0f;

			if (currentState == GameState::Title) {
				idleTime++;
				if (idleTime > 300) {
					currentState = GameState::Demo;
					btnRed->Visible = false;
					btnGreen->Visible = false;
					btnBlue->Visible = false;
					ResetPlayer();
				}
				this->Invalidate();
				return;
			}

			// プレイモードかつ最初のクリック前なら、描画だけ更新して時を止める
			if (currentState == GameState::Playing && !isGameStarted) {
				this->Invalidate();
				return;
			}

			// クールタイムのカウントダウン
			if (shootCooldown > 0) {
				shootCooldown--;
			}

			// デモAI用（AIも同じルールに従う）
			if (currentState == GameState::Demo) {
				float closestObsDist = 9999.0f;
				Entity targetObs;
				bool obsFound = false;
				for (int i = 0; i < worldObjects->Length; i++) {
					float dist = worldObjects[i].rect.X - px;
					if (dist > -60.0f && dist < closestObsDist) {
						closestObsDist = dist;
						targetObs = worldObjects[i];
						obsFound = true;
					}
				}

				if (!isAttached && canShoot && shootCooldown == 0 && currentEnergy > 0) {
					bool shouldHook = false;
					float targetAnchorX = px + 180.0f;

					if (obsFound && closestObsDist < 260.0f) {
						if (py > targetObs.rect.Y - 130.0f) {
							shouldHook = true;
							targetAnchorX = targetObs.rect.X + targetObs.rect.Width + 50.0f;
						}
					}
					else if (py > 280.0f) {
						shouldHook = true;
						targetAnchorX = px + 160.0f;
					}

					if (shouldHook) {
						anchorX = targetAnchorX;
						anchorY = 20.0f;
						float dx = px - anchorX;
						float dy = py - anchorY;
						ropeLength = (float)Math::Sqrt(dx * dx + dy * dy) * 0.72f;
						isAttached = true;
						shootCooldown = MAX_COOLDOWN;
					}
				}
				else if (isAttached) {
					bool overObstacle = (obsFound && px > targetObs.rect.X - 30.0f && px < targetObs.rect.X + targetObs.rect.Width + 20.0f);
					bool movingForward = (px - oldx) > 1.0f;

					if (!overObstacle && movingForward) {
						if (px > anchorX - 10.0f || py < 160.0f) {
							isAttached = false;
						}
					}

					if (py > 450.0f) {
						isAttached = false;
					}
				}
			}

			if (buffTimer > 0.0f) {
				buffTimer -= 1.0f;
				if (buffTimer <= 0.0f) {
					currentBuff = BuffType::None;
				}
			}

			currentEnergy -= 1.0f;
			if (currentEnergy <= 0.0f) {
				currentEnergy = 0.0f;
				isAttached = false;
				GameOver();
				return;
			}

			int currentDist = (int)((px - 100) / 10);
			if (currentDist > score) score = currentDist;

			float difficultyMult = 1.0f + (score / 1000.0f);
			if (difficultyMult > 2.5f) difficultyMult = 2.5f;

			float vx = (px - oldx) * damp;
			float vy = (py - oldy) * damp;
			oldx = px; oldy = py;

			px += vx;
			py += vy + gravity;

			// ウィンドウ内移動制限
			if (px < cameraX + 15.0f) {
				px = cameraX + 15.0f;
				oldx = px;
			}
			if (px > cameraX + 800.0f - 15.0f) {
				px = cameraX + 800.0f - 15.0f;
				oldx = px;
			}
			if (py < 15.0f) {
				py = 15.0f;
				oldy = py;
			}

			if (isAttached) {
				float dx = px - anchorX;
				float dy = py - anchorY;
				float dist = (float)Math::Sqrt(dx * dx + dy * dy);
				if (dist > ropeLength) {
					float stretch = dist - ropeLength;
					float force = stretch * springK;
					px -= (dx / dist) * force;
					py -= (dy / dist) * force;
					if (py > anchorY && vx > 0) px += 0.15f;
				}
			}

			if (!isAttached && px > startPlatform.X && px < startPlatform.X + startPlatform.Width) {
				if (py + 15 > startPlatform.Y && py < startPlatform.Y + startPlatform.Height) {
					py = startPlatform.Y - 15;
					oldy = py;
				}
			}

			for (int i = 0; i < worldObjects->Length; i++) {
				if (worldObjects[i].isMoving) {
					worldObjects[i].rect.Y = (int)(worldObjects[i].startY + Math::Sin(timeCount * worldObjects[i].speed + worldObjects[i].offset) * worldObjects[i].range);
				}

				if (worldObjects[i].rect.X < cameraX - 200) {
					int furthestX = 0;
					for (int j = 0; j < worldObjects->Length; j++) {
						if (worldObjects[j].rect.X > furthestX) furthestX = worldObjects[j].rect.X;
					}

					int minGap = 250;
					int maxGap = (int)(600 / difficultyMult);
					if (maxGap <= minGap) maxGap = minGap + 100;

					worldObjects[i].rect.X = furthestX + rnd->Next(minGap, maxGap);
					worldObjects[i].startY = (float)rnd->Next(150, 450);
					worldObjects[i].rect.Y = (int)worldObjects[i].startY;
					worldObjects[i].range = (float)rnd->Next(50, 150) * difficultyMult;
					worldObjects[i].speed = (0.02f + (float)rnd->NextDouble() * 0.03f) * difficultyMult;
					worldObjects[i].offset = (float)(rnd->NextDouble() * 10.0);
				}

				if (px + 12 > worldObjects[i].rect.X && px - 12 < worldObjects[i].rect.X + worldObjects[i].rect.Width &&
					py + 12 > worldObjects[i].rect.Y && py - 12 < worldObjects[i].rect.Y + worldObjects[i].rect.Height) {

					if (buffTimer > 0.0f && currentBuff == BuffType::Invincible) {
						continue;
					}

					GameOver();
					return;
				}
			}

			for (int i = 0; i < bugs->Length; i++) {
				if (bugs[i].rect.X < cameraX - 100) {
					int furthestX = (int)px;
					for (int j = 0; j < bugs->Length; j++) {
						if (bugs[j].rect.X > furthestX) furthestX = bugs[j].rect.X;
					}
					bugs[i].rect.X = furthestX + rnd->Next(250, 500);
					bugs[i].baseY = (float)rnd->Next(100, 450);
					bugs[i].isActive = true;

					if (score >= nextSpecialScore) {
						bugs[i].isSpecial = true;
						nextSpecialScore += 250;
					}
					else {
						bugs[i].isSpecial = false;
					}
				}

				if (bugs[i].isActive) {
					float bCenterX = bugs[i].rect.X + 8.0f;
					float bCenterY = bugs[i].rect.Y + 8.0f;
					float dx = px - bCenterX;
					float dy = py - bCenterY;
					float distSq = dx * dx + dy * dy;

					bool isMagnetBuff = (buffTimer > 0.0f && currentBuff == BuffType::SuperMagnet);
					float magnetRangeSq = isMagnetBuff ? 160000.0f : 22500.0f;
					float pullSpeed = isMagnetBuff ? 15.0f : 7.0f;

					if (distSq < magnetRangeSq) {
						float dist = (float)Math::Sqrt(distSq);
						if (dist > 0) {
							bugs[i].rect.X += (int)((dx / dist) * pullSpeed);
							bugs[i].rect.Y += (int)((dy / dist) * pullSpeed);
						}
					}
					else {
						bugs[i].rect.Y = (int)(bugs[i].baseY + Math::Sin(timeCount * 0.1f + i) * 20.0f);
					}

					if (distSq < 400.0f) {
						bugs[i].isActive = false;

						if (bugs[i].isSpecial) {
							int dice = rnd->Next(1, 4);
							if (dice == 1) currentBuff = BuffType::Invincible;
							else if (dice == 2) currentBuff = BuffType::DoubleScore;
							else if (dice == 3) currentBuff = BuffType::SuperMagnet;

							buffTimer = 300.0f;
						}
						else {
							if (buffTimer > 0.0f && currentBuff == BuffType::DoubleScore) {
								bonusScore += 100;
							}
							else {
								bonusScore += 50;
							}
						}

						currentEnergy = MAX_ENERGY;
						shootCooldown = 0; // ハエを食べたらクールタイム即リセット
					}
				}
			}

			float targetCam = px - 300;
			if (targetCam < 0) targetCam = 0;

			float nextCam = cameraX + (targetCam - cameraX) * 0.1f;
			if (nextCam > cameraX) {
				cameraX = nextCam;
			}

			if (py > 600) { GameOver(); return; }

			this->Invalidate();
		}

		void OnMouseMove(Object^ sender, MouseEventArgs^ e) {
			if (currentState == GameState::Title) {
				idleTime = 0;
			}
		}

		void OnMouseDown(Object^ sender, MouseEventArgs^ e) {
			// フルスクリーンの拡大率を逆算して、800x600基準の仮想マウス座標に変換
			float mX = (float)e->X / scaleX;
			float mY = (float)e->Y / scaleY;

			if (currentState == GameState::Title) {
				idleTime = 0;
				if (mY < 450) {
					currentState = GameState::Playing;
					isGameStarted = false; // プレイ開始時はまだ未スタート
					btnRed->Visible = false;
					btnGreen->Visible = false;
					btnBlue->Visible = false;
				}
				return;
			}

			if (currentState == GameState::Demo) {
				currentState = GameState::Playing;
				ResetPlayer();
				isGameStarted = false; // デモから抜けた時も未スタート
				return;
			}

			// プレイ中でまだ始まっていない場合、最初のこのクリックで動かす
			if (currentState == GameState::Playing && !isGameStarted) {
				isGameStarted = true;
			}

			// クールタイム中は発射不可
			if (shootCooldown > 0 || currentEnergy <= 0) return;

			anchorX = mX + cameraX;
			anchorY = mY;
			float dx = px - anchorX;
			float dy = py - anchorY;
			ropeLength = (float)Math::Sqrt(dx * dx + dy * dy) * 0.8f;
			isAttached = true;

			shootCooldown = MAX_COOLDOWN; // 30フレーム（0.5秒）セット
		}

		void OnMouseUp(Object^ sender, MouseEventArgs^ e) {
			if (currentState == GameState::Demo) return;
			isAttached = false;
		}

		void OnPaint(Object^ sender, PaintEventArgs^ e) {
			Graphics^ g = e->Graphics;
			g->SmoothingMode = SmoothingMode::AntiAlias;

			g->Clear(Color::FromArgb(20, 20, 30));

			// 【フルスクリーン対応】画面サイズに応じた倍率を毎フレーム計算して全体に適用
			scaleX = (float)this->ClientSize.Width / 800.0f;
			scaleY = (float)this->ClientSize.Height / 600.0f;
			g->ScaleTransform(scaleX, scaleY);

			// スケールだけが適用された基本状態のマトリックスを保存
			Drawing2D::Matrix^ baseMatrix = g->Transform;

			if (currentState == GameState::Title) {
				DrawCityBackground(g, 0);
				System::Drawing::Font^ titleFont = gcnew System::Drawing::Font("Arial Black", 48, FontStyle::Bold);
				g->DrawString("SPIDER HOOK GAME", titleFont, Brushes::DarkSlateBlue, 10, 150);
				System::Drawing::Font^ infoFont = gcnew System::Drawing::Font("Consolas", 18, FontStyle::Regular);
				g->DrawString("BEST SCORE: " + highScore + " Pt", infoFont, Brushes::Gray, 260, 260);

				if (((int)timeCount / 30) % 2 == 0) {
					g->DrawString(">> CLICK TO START <<", infoFont, Brushes::Tomato, 260, 350);
				}

				// 操作ヒントをひっそり配置
				System::Drawing::Font^ hintFont = gcnew System::Drawing::Font("Arial", 10, FontStyle::Regular);
				g->DrawString("[F11] Toggle Fullscreen", hintFont, Brushes::DimGray, 20, 560);

				DrawSpider(g, 400, 425, playerColor);

			}
			else {
				DrawCityBackground(g, cameraX);

				int totalScore = score + bonusScore;
				String^ scoreText = String::Format("Score: {0} Pt   [ Best: {1} Pt ]", totalScore, highScore);
				Brush^ scoreBrush = Brushes::White;
				if (currentBuff == BuffType::DoubleScore && ((int)timeCount % 10 < 5)) {
					scoreBrush = Brushes::Gold;
				}
				g->DrawString(scoreText, gcnew System::Drawing::Font("Consolas", 14, FontStyle::Bold), scoreBrush, 20, 20);

				if (currentState == GameState::Demo) {
					if (((int)timeCount / 20) % 2 == 0) {
						g->DrawString("DEMO PLAY - CLICK TO START!", gcnew System::Drawing::Font("Arial Black", 20, FontStyle::Bold),
							Brushes::Tomato, 180, 70);
					}
				}

				// ゲーム開始前の待機中案内メッセージ（CLICK TO START!）
				if (currentState == GameState::Playing && !isGameStarted) {
					if (((int)timeCount / 25) % 2 == 0) {
						g->DrawString("CLICK TO START!", gcnew System::Drawing::Font("Arial Black", 26, FontStyle::Bold),
							Brushes::Orange, 240, 120);
					}
				}

				if (buffTimer > 0.0f) {
					String^ buffName = "";
					Brush^ buffBrush = Brushes::Gold;

					if (currentBuff == BuffType::Invincible) {
						buffName = "INVINCIBLE!";
						buffBrush = Brushes::Cyan;
					}
					else if (currentBuff == BuffType::DoubleScore) {
						buffName = "SCORE x2!";
						buffBrush = Brushes::Gold;
					}
					else if (currentBuff == BuffType::SuperMagnet) {
						buffName = "SUPER MAGNET!";
						buffBrush = Brushes::Magenta;
					}

					String^ buffText = String::Format("{0} [ {1:F1}s ]", buffName, buffTimer / 60.0f);
					System::Drawing::Font^ buffFont = gcnew System::Drawing::Font("Arial Black", 16, FontStyle::Italic | FontStyle::Bold);

					SizeF textSize = g->MeasureString(buffText, buffFont);
					float textX = 800.0f - textSize.Width - 20.0f;

					g->DrawString(buffText, buffFont, buffBrush, textX, 20.0f);
				}

				// カメラのスクロール効果を適用
				g->TranslateTransform(-cameraX, 0);

				// ────────────────────────────────────────────────────────
				// ★ 背景（画面最上部）の天井・太い梁（鉄骨）の無限スクロール描画
				// ────────────────────────────────────────────────────────
				// 梁のベース（【エラー対策】引数を数値で指定し、オーバーロードの衝突を解消）
				g->FillRectangle(gcnew SolidBrush(Color::FromArgb(65, 65, 75)), cameraX - 50.0f, 0.0f, 900.0f, 20.0f);
				g->FillRectangle(gcnew SolidBrush(Color::FromArgb(45, 45, 55)), cameraX - 50.0f, 20.0f, 900.0f, 4.0f); // 影ライン

				// 鉄骨トラス構造（X状のフレーム）
				Pen^ steelPen = gcnew Pen(Color::FromArgb(95, 95, 105), 2);
				int trussWidth = 80; // 鉄骨1マスの横幅
				int startTruss = (int)(cameraX / trussWidth) - 1;
				int endTruss = startTruss + (800 / trussWidth) + 3;
				for (int i = startTruss; i < endTruss; i++) {
					int tx = i * trussWidth;
					g->DrawLine(steelPen, tx, 0, tx + trussWidth, 20); // ＼ の骨組み
					g->DrawLine(steelPen, tx + trussWidth, 0, tx, 20); // ／ の骨組み
					g->DrawLine(steelPen, tx, 0, tx, 20);               // ｜ 縦の柱
				}
				// ────────────────────────────────────────────────────────

				// スタート地点の足場（【エラー対策】こちらも引数をバラしてオーバーロードの衝突を解消）
				g->FillRectangle(Brushes::DarkSlateGray, (float)startPlatform.X, (float)startPlatform.Y, (float)startPlatform.Width, (float)startPlatform.Height);

				HatchBrush^ dangerBrush = gcnew HatchBrush(HatchStyle::BackwardDiagonal, Color::Black, Color::Yellow);
				for each (Entity ent in worldObjects) {
					int glowSize = (int)(Math::Sin(timeCount * 0.1) * 3) + 5;

					// 【エラー対策】glowRect, 障害物本体, 輪郭線すべてバラして渡すことで曖昧性を100%排除
					g->FillRectangle(gcnew SolidBrush(Color::FromArgb(80, 255, 0, 0)),
						(float)(ent.rect.X - glowSize), (float)(ent.rect.Y - glowSize),
						(float)(ent.rect.Width + glowSize * 2), (float)(ent.rect.Height + glowSize * 2));

					g->FillRectangle(dangerBrush, (float)ent.rect.X, (float)ent.rect.Y, (float)ent.rect.Width, (float)ent.rect.Height);
					g->DrawRectangle(gcnew Pen(Color::Red, 2), (float)ent.rect.X, (float)ent.rect.Y, (float)ent.rect.Width, (float)ent.rect.Height);
				}

				for (int i = 0; i < bugs->Length; i++) {
					if (bugs[i].isActive) {
						int bx = bugs[i].rect.X;
						int by = bugs[i].rect.Y;

						// 開始前はハエの羽ばたきアニメを固定する
						float animTime = isGameStarted ? timeCount : 0.0f;
						int wingY = by - 4 + (int)(Math::Sin(animTime * 1.5f) * 3.0f);

						if (bugs[i].isSpecial) {
							g->FillEllipse(Brushes::Gold, bx - 12, wingY - 2, 16, 12);
							g->FillEllipse(Brushes::Gold, bx + 12, wingY - 2, 16, 12);
							g->FillEllipse(Brushes::OrangeRed, bx - 2, by - 2, 20, 20);
							g->FillEllipse(Brushes::Yellow, bx + 2, by + 2, 6, 6);
							g->FillEllipse(Brushes::Yellow, bx + 12, by + 2, 6, 6);
						}
						else {
							g->FillEllipse(Brushes::LightBlue, bx - 8, wingY, 12, 10);
							g->FillEllipse(Brushes::LightBlue, bx + 12, wingY, 12, 10);
							g->FillEllipse(Brushes::Black, bx, by, 16, 16);
							g->FillEllipse(Brushes::Red, bx + 2, by + 2, 4, 4);
							g->FillEllipse(Brushes::Red, bx + 10, by + 2, 4, 4);
						}
					}
				}

				if (isAttached) {
					Pen^ ropePen = gcnew Pen(Color::FromArgb(180, Color::White), 2);
					ropePen->DashStyle = DashStyle::Dash;
					float midX = (anchorX + px) / 2.0f;
					float midY = (anchorY + py) / 2.0f;
					float dx = px - anchorX;
					float dy = py - anchorY;
					float dist = (float)Math::Sqrt(dx * dx + dy * dy);
					float droopAmount = 20.0f;
					if (dist < ropeLength) droopAmount += (ropeLength - dist) * 0.5f;

					float controlY = midY + droopAmount;
					Point cp = Point((int)midX, (int)controlY);
					array<Point>^ bezierPoints = { Point((int)anchorX, (int)anchorY), cp, cp, Point((int)px, (int)py) };
					g->DrawBeziers(ropePen, bezierPoints);
					g->FillEllipse(Brushes::White, (int)anchorX - 4, (int)anchorY - 4, 8, 8);
				}

				// クールタイム中のクモの色変化
				Color currentSpiderColor = playerColor;
				if (shootCooldown > 0) {
					currentSpiderColor = Color::FromArgb(110, 115, 125); // 撃てない間はグレー
				}
				DrawSpider(g, px, py, currentSpiderColor);

				if (buffTimer > 0.0f) {
					Color auraColor = Color::FromArgb(120, Color::Gold);
					if (currentBuff == BuffType::Invincible) auraColor = Color::FromArgb(120, Color::Cyan);
					else if (currentBuff == BuffType::DoubleScore) auraColor = Color::FromArgb(120, Color::Gold);
					else if (currentBuff == BuffType::SuperMagnet) auraColor = Color::FromArgb(120, Color::Magenta);

					int auraSize = 55 + (int)(Math::Sin(timeCount * 0.3f) * 8);
					g->DrawEllipse(gcnew Pen(auraColor, 3), (int)(px - auraSize / 2), (int)(py - auraSize / 2), auraSize, auraSize);
				}

				int barWidth = 40;
				int barX = (int)px - barWidth / 2;
				int barY = (int)py - 54;
				g->FillRectangle(Brushes::DimGray, (float)barX, (float)barY, (float)barWidth, 5.0f);

				float ratio = currentEnergy / MAX_ENERGY;
				if (ratio < 0.0f) ratio = 0.0f;

				Brush^ gaugeBrush = Brushes::LimeGreen;
				if (ratio < 0.3f) {
					gaugeBrush = ((int)timeCount % 10 < 5) ? Brushes::Red : Brushes::DarkRed;
				}
				g->FillRectangle(gaugeBrush, (float)barX, (float)barY, (float)(barWidth * ratio), 5.0f);

				// 【フルスクリーン補正】カメラ移動だけをリセットし、解像度スケールは維持する
				g->Transform = baseMatrix;

				// マウスカーソル（照準）の周りにクールタイムメーターを描画
				if (shootCooldown > 0 && currentState == GameState::Playing) {
					Point mousePos = this->PointToClient(System::Windows::Forms::Control::MousePosition);

					// 実際のマウス位置を、拡大率で逆算して800x600内の仮想座標に直す
					float mX = (float)mousePos.X / scaleX;
					float mY = (float)mousePos.Y / scaleY;

					float radius = 16.0f;
					float gx = mX - radius;
					float gy = mY - radius;

					g->DrawEllipse(gcnew Pen(Color::FromArgb(50, Color::White), 1.5f), gx, gy, radius * 2.0f, radius * 2.0f);

					float progress = (float)shootCooldown / MAX_COOLDOWN;
					float sweepAngle = progress * 360.0f;

					g->DrawArc(gcnew Pen(Color::Tomato, 3.5f), gx, gy, radius * 2.0f, radius * 2.0f, -90.0f, sweepAngle);
				}
			}
		}

		void InitializeComponent(void) {
			this->SuspendLayout();
			this->Name = L"MyForm";
			this->Text = L"Spider Hook Game";
			this->ResumeLayout(false);
		}
	};
}