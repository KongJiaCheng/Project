const express = require('express');
const stripe = require('stripe')(process.env.STRIPE_SECRET_KEY || 'sk_test_51PTestSecretKeyPlaceholder');
const mysql = require('mysql2/promise');
const fs = require('fs');
const path = require('path');
require('dotenv').config();

const app = express();
const port = process.env.PORT || 3000;

// Set up connection credentials using SSL cert
const dbConfig = {
  host: process.env.DB_HOST || "mysql-308a256a-econovadb.h.aivencloud.com",
  port: parseInt(process.env.DB_PORT || "22311"),
  user: process.env.DB_USER || "avnadmin",
  password: process.env.DB_PASSWORD || "AVNS_YV3eowXWv3Q2tArX8fd",
  database: process.env.DB_NAME || "mmorpg",
  ssl: {
    ca: fs.readFileSync(path.join(__dirname, 'ca.pem')),
    rejectUnauthorized: false
  }
};

// Create a Checkout Session
app.get('/checkout', async (req, res) => {
  const { user, gold, price, tier } = req.query;
  if (!user || !gold || !price || !tier) {
    return res.status(400).send("Missing checkout parameters: user, gold, price, tier are required.");
  }
  
  try {
    const session = await stripe.checkout.sessions.create({
      payment_method_types: ['card'],
      line_items: [{
        price_data: {
          currency: 'usd',
          product_data: {
            name: `${tier} (${gold} Gold Coins)`,
            description: `In-Game virtual currency for RAS Game character: ${user}`,
          },
          unit_amount: Math.round(parseFloat(price) * 100), // In cents
        },
        quantity: 1,
      }],
      mode: 'payment',
      metadata: {
        username: user,
        gold: gold
      },
      success_url: `${req.protocol}://${req.get('host')}/success?user=${encodeURIComponent(user)}&gold=${gold}`,
      cancel_url: `${req.protocol}://${req.get('host')}/cancel`,
    });
    
    // Redirect browser directly to Stripe's secure Checkout Page
    res.redirect(303, session.url);
  } catch (err) {
    console.error("Error creating Stripe Checkout Session:", err);
    res.status(500).send(`Stripe Error: ${err.message}`);
  }
});

// Secure Stripe Webhook Endpoint
app.post('/webhook', express.raw({ type: 'application/json' }), async (req, res) => {
  const sig = req.headers['stripe-signature'];
  let event;
  
  try {
    event = stripe.webhooks.constructEvent(req.body, sig, process.env.STRIPE_WEBHOOK_SECRET || 'whsec_testPlaceholder');
  } catch (err) {
    console.error('Webhook signature verification failed:', err.message);
    return res.status(400).send(`Webhook Error: ${err.message}`);
  }
  
  if (event.type === 'checkout.session.completed') {
    const session = event.data.object;
    const username = session.metadata.username;
    const goldAmount = parseInt(session.metadata.gold);
    
    if (username && goldAmount) {
      try {
        console.log(`[STRIPE] Received successful checkout session for [${username}] - +${goldAmount} Gold`);
        
        // 1. Update gold directly in MariaDB
        const connection = await mysql.createConnection(dbConfig);
        const [result] = await connection.execute(
          'UPDATE Accounts SET Gold = Gold + ? WHERE Username = ?',
          [goldAmount, username]
        );
        await connection.end();
        console.log(`[SUCCESS] Database Updated: Added ${goldAmount} Gold to user [${username}]`);
        
        // 2. Write the sync receipt local file for C++ game to auto-sync instantly!
        const syncReceipt = { username, gold: goldAmount, verified: true };
        fs.writeFileSync(
          path.join(__dirname, '../RASGame/RASGame/payment_success.json'),
          JSON.stringify(syncReceipt, null, 2)
        );
        console.log(`[SUCCESS] Sync receipt written to game folder.`);
      } catch (dbErr) {
        console.error('[ERROR] Database update failed:', dbErr);
      }
    }
  }
  res.json({ received: true });
});

// Success Page template
app.get('/success', (req, res) => {
  const { user, gold } = req.query;
  res.send(`
    <html>
      <head>
        <title>Payment Successful</title>
        <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@400;600;800&display=swap" rel="stylesheet">
        <style>
          body {
            background: radial-gradient(circle at center, #1e293b, #0f172a);
            color: #ffffff;
            font-family: 'Outfit', sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
          }
          .card {
            background: rgba(30, 41, 59, 0.7);
            border: 2px solid #00f2fe;
            border-radius: 16px;
            padding: 40px;
            text-align: center;
            box-shadow: 0 8px 32px rgba(0, 242, 254, 0.2);
            backdrop-filter: blur(8px);
            max-width: 400px;
          }
          h1 { color: #00f2fe; margin-bottom: 10px; }
          p { color: #cbd5e1; margin-bottom: 20px; }
          .highlight { font-weight: bold; color: #ffd700; }
          .btn {
            display: inline-block;
            background: #00f2fe;
            color: #0f172a;
            padding: 10px 20px;
            border-radius: 8px;
            text-decoration: none;
            font-weight: bold;
            transition: 0.3s;
          }
          .btn:hover { background: #00b4d8; }
        </style>
      </head>
      <body>
        <div class="card">
          <h1>Payment Successful!</h1>
          <p>Thank you <span class="highlight">${user}</span>! Your purchase of <span class="highlight">${gold} Gold</span> has been completed and securely verified.</p>
          <p>You can return to the game client, and your balance will automatically sync.</p>
          <a href="#" onclick="window.close()" class="btn">Return to Game</a>
        </div>
      </body>
    </html>
  `);
});

// Cancel Page template
app.get('/cancel', (req, res) => {
  res.send(`
    <html>
      <head>
        <title>Payment Cancelled</title>
        <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@400;600;800&display=swap" rel="stylesheet">
        <style>
          body {
            background: radial-gradient(circle at center, #1e293b, #0f172a);
            color: #ffffff;
            font-family: 'Outfit', sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
          }
          .card {
            background: rgba(30, 41, 59, 0.7);
            border: 2px solid #ef4444;
            border-radius: 16px;
            padding: 40px;
            text-align: center;
            box-shadow: 0 8px 32px rgba(239, 68, 68, 0.2);
            backdrop-filter: blur(8px);
            max-width: 400px;
          }
          h1 { color: #ef4444; margin-bottom: 10px; }
          p { color: #cbd5e1; margin-bottom: 20px; }
          .btn {
            display: inline-block;
            background: #ef4444;
            color: #ffffff;
            padding: 10px 20px;
            border-radius: 8px;
            text-decoration: none;
            font-weight: bold;
            transition: 0.3s;
          }
          .btn:hover { background: #dc2626; }
        </style>
      </head>
      <body>
        <div class="card">
          <h1>Payment Cancelled</h1>
          <p>The transaction was cancelled. No charges were made.</p>
          <a href="#" onclick="window.close()" class="btn">Close Tab</a>
        </div>
      </body>
    </html>
  `);
});

app.listen(port, () => {
  console.log(`[INFO] Stripe Payment Server running on http://localhost:${port}`);
  console.log(`[INFO] Ready to receive Stripe webhook callbacks at http://localhost:${port}/webhook`);
});
