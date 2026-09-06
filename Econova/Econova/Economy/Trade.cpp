#include "Trade.h"
#include <algorithm>

void drawP2PTrade(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::string& p2pPartner,
    const TradeSlot myP2PSlots[4],
    const std::string& myP2PGoldInput,
    bool p2pGoldTyping,
    bool p2pMyReady,
    const std::vector<P2PItemDisplay>& partnerItems,
    int partnerGold,
    bool partnerReady,
    const std::string& p2pStatusMsg,
    const sf::Color& p2pStatusColor,
    std::function<const sf::Texture*(int id)> getItemTexture,
    std::function<std::string(int id)> getItemName
) {
    float tW = 700.f, tH = 440.f;
    sf::FloatRect tR({ (width - tW) / 2.f, (height - tH) / 2.f }, { tW, tH });

    sf::RectangleShape bg({ tW, tH });
    bg.setPosition(tR.position);
    bg.setFillColor(sf::Color(18, 18, 26, 252));
    bg.setOutlineThickness(2.f);
    bg.setOutlineColor(sf::Color(100, 200, 255));
    window.draw(bg);

    // Header
    sf::RectangleShape hdr({ tW, 48.f });
    hdr.setPosition(tR.position);
    hdr.setFillColor(sf::Color(30, 55, 100));
    window.draw(hdr);
    sf::Text htxt(uiFont, "P2P TRADE: You  <=>  " + p2pPartner, 22);
    htxt.setStyle(sf::Text::Bold);
    htxt.setFillColor(sf::Color(180, 230, 255));
    htxt.setPosition({ tR.position.x + 16.f, tR.position.y + 10.f });
    window.draw(htxt);

    // Divider
    sf::RectangleShape div({ 2.f, tH - 48.f });
    div.setPosition({ tR.position.x + tW / 2.f, tR.position.y + 48.f });
    div.setFillColor(sf::Color(80, 120, 180, 160));
    window.draw(div);

    // ---- LEFT: MY OFFER ----
    sf::Text myLabel(uiFont, "YOUR OFFER", 16);
    myLabel.setFillColor(sf::Color(100, 220, 100));
    myLabel.setPosition({ tR.position.x + 20.f, tR.position.y + 54.f });
    window.draw(myLabel);

    for (int i = 0; i < 4; i++) {
        sf::Vector2f sp = { tR.position.x + 20.f + i * 74.f, tR.position.y + 80.f };
        sf::RectangleShape slot({ 64.f, 64.f });
        slot.setPosition(sp);
        slot.setFillColor(myP2PSlots[i].tex ? sf::Color(40, 70, 40) : sf::Color(45, 45, 55));
        slot.setOutlineThickness(2.f);
        slot.setOutlineColor(myP2PSlots[i].tex ? sf::Color(80, 220, 80) : sf::Color(70, 70, 90));
        window.draw(slot);
        if (myP2PSlots[i].tex) {
            sf::Sprite s(*myP2PSlots[i].tex);
            sf::Vector2u sz = s.getTexture().getSize();
            float sc = 48.f / std::max(sz.x, sz.y);
            s.setScale({ sc, sc });
            s.setPosition({ sp.x + 8.f, sp.y + 8.f });
            window.draw(s);
            sf::Text ct(uiFont, "x" + std::to_string(myP2PSlots[i].count), 13);
            ct.setFillColor(sf::Color::Yellow);
            ct.setPosition({ sp.x + 2.f, sp.y + 48.f });
            window.draw(ct);
            sf::Text nm(uiFont, getItemName(myP2PSlots[i].id), 11);
            nm.setFillColor(sf::Color(200, 200, 200));
            nm.setPosition({ sp.x, sp.y + 66.f });
            window.draw(nm);
        } else {
            sf::Text em(uiFont, "+", 26);
            em.setFillColor(sf::Color(60, 60, 80));
            em.setPosition({ sp.x + 22.f, sp.y + 16.f });
            window.draw(em);
        }
    }

    // My Gold offer
    sf::Text glbl(uiFont, "Gold offer:", 15);
    glbl.setFillColor(sf::Color(200, 200, 200));
    glbl.setPosition({ tR.position.x + 20.f, tR.position.y + 150.f });
    window.draw(glbl);
    sf::RectangleShape gbox({ 100.f, 34.f });
    gbox.setPosition({ tR.position.x + 20.f, tR.position.y + 168.f });
    gbox.setFillColor(sf::Color(30, 30, 30));
    gbox.setOutlineThickness(2.f);
    gbox.setOutlineColor(p2pGoldTyping ? sf::Color::Yellow : sf::Color(80, 120, 180));
    window.draw(gbox);
    sf::Text gtxt(uiFont, myP2PGoldInput.empty() ? "0" : myP2PGoldInput, 17);
    gtxt.setFillColor(sf::Color(255, 210, 50));
    gtxt.setPosition({ tR.position.x + 26.f, tR.position.y + 172.f });
    window.draw(gtxt);

    // My ready status
    sf::Text myReadyTxt(uiFont, p2pMyReady ? "YOU: READY" : "YOU: NOT READY", 14);
    myReadyTxt.setFillColor(p2pMyReady ? sf::Color(80, 220, 80) : sf::Color(180, 80, 80));
    myReadyTxt.setPosition({ tR.position.x + 20.f, tR.position.y + 215.f });
    window.draw(myReadyTxt);

    // ---- RIGHT: PARTNER'S OFFER ----
    float rx = tR.position.x + tW / 2.f + 20.f;
    sf::Text pLabel(uiFont, p2pPartner + "'s OFFER", 16);
    pLabel.setFillColor(sf::Color(100, 180, 255));
    pLabel.setPosition({ rx, tR.position.y + 54.f });
    window.draw(pLabel);

    for (int i = 0; i < (int)partnerItems.size() && i < 4; i++) {
        sf::Vector2f sp = { rx + i * 74.f, tR.position.y + 80.f };
        const sf::Texture* tex = getItemTexture(partnerItems[i].id);
        sf::RectangleShape slot({ 64.f, 64.f });
        slot.setPosition(sp);
        slot.setFillColor(tex ? sf::Color(40, 50, 80) : sf::Color(45, 45, 55));
        slot.setOutlineThickness(2.f);
        slot.setOutlineColor(sf::Color(80, 130, 220));
        window.draw(slot);
        if (tex) {
            sf::Sprite s(*tex);
            sf::Vector2u sz = s.getTexture().getSize();
            float sc = 48.f / std::max(sz.x, sz.y);
            s.setScale({ sc, sc });
            s.setPosition({ sp.x + 8.f, sp.y + 8.f });
            window.draw(s);
            sf::Text ct(uiFont, "x" + std::to_string(partnerItems[i].count), 13);
            ct.setFillColor(sf::Color::Yellow);
            ct.setPosition({ sp.x + 2.f, sp.y + 48.f });
            window.draw(ct);
            sf::Text nm(uiFont, getItemName(partnerItems[i].id), 11);
            nm.setFillColor(sf::Color(200, 200, 200));
            nm.setPosition({ sp.x, sp.y + 66.f });
            window.draw(nm);
        } else {
            sf::Text em(uiFont, "-", 26);
            em.setFillColor(sf::Color(60, 60, 80));
            em.setPosition({ sp.x + 26.f, sp.y + 16.f });
            window.draw(em);
        }
    }

    // Partner Gold
    sf::Text pglbl(uiFont, "Gold offer:", 15);
    pglbl.setFillColor(sf::Color(200, 200, 200));
    pglbl.setPosition({ rx, tR.position.y + 150.f });
    window.draw(pglbl);
    sf::RectangleShape pgbox({ 100.f, 34.f });
    pgbox.setPosition({ rx, tR.position.y + 168.f });
    pgbox.setFillColor(sf::Color(30, 30, 30));
    pgbox.setOutlineThickness(2.f);
    pgbox.setOutlineColor(sf::Color(80, 120, 180));
    window.draw(pgbox);
    sf::Text pgtxt(uiFont, std::to_string(partnerGold), 17);
    pgtxt.setFillColor(sf::Color(255, 210, 50));
    pgtxt.setPosition({ rx + 26.f, tR.position.y + 172.f });
    window.draw(pgtxt);

    // Partner ready status
    sf::Text pReadyTxt(uiFont, partnerReady ? p2pPartner + ": READY" : p2pPartner + ": NOT READY", 14);
    pReadyTxt.setFillColor(partnerReady ? sf::Color(80, 220, 80) : sf::Color(180, 80, 80));
    pReadyTxt.setPosition({ rx, tR.position.y + 215.f });
    window.draw(pReadyTxt);

    // ---- STATUS MESSAGE ----
    if (!p2pStatusMsg.empty()) {
        sf::Text st(uiFont, p2pStatusMsg, 15);
        st.setFillColor(p2pStatusColor);
        st.setPosition({ tR.position.x + 20.f, tR.position.y + 320.f });
        window.draw(st);
    }

    // ---- ACTION BUTTONS ----
    float btnY = tR.position.y + 360.f;

    // READY button
    sf::RectangleShape readyBtn({ 130.f, 44.f });
    readyBtn.setPosition({ tR.position.x + 20.f, btnY });
    readyBtn.setFillColor(p2pMyReady ? sf::Color(40, 160, 70) : sf::Color(60, 110, 180));
    readyBtn.setOutlineThickness(2.f);
    readyBtn.setOutlineColor(sf::Color::White);
    window.draw(readyBtn);
    sf::Text rdyTxt(uiFont, p2pMyReady ? "UNREADY" : "READY", 17);
    rdyTxt.setStyle(sf::Text::Bold);
    rdyTxt.setPosition({ tR.position.x + (p2pMyReady ? 32.f : 44.f), btnY + 10.f });
    window.draw(rdyTxt);

    // CANCEL button
    sf::RectangleShape cancelBtn({ 120.f, 44.f });
    cancelBtn.setPosition({ tR.position.x + tW - 140.f, btnY });
    cancelBtn.setFillColor(sf::Color(180, 50, 50));
    cancelBtn.setOutlineThickness(2.f);
    cancelBtn.setOutlineColor(sf::Color::White);
    window.draw(cancelBtn);
    sf::Text cclTxt(uiFont, "CANCEL", 17);
    cclTxt.setStyle(sf::Text::Bold);
    cclTxt.setPosition({ tR.position.x + tW - 126.f, btnY + 10.f });
    window.draw(cclTxt);
}

void drawNPCTrade(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const TradeSlot playerTradeSlots[4],
    const TradeSlot npcTradeSlots[4],
    bool isSellingToNPC,
    int npcTradeGold,
    const std::string& npcGoldInput,
    const std::string& playerGoldInput,
    bool playerGoldTyping,
    int currency,
    bool hasBargained,
    bool bargainAccepted,
    const std::string& bargainStatus,
    const sf::Color& bargainColor,
    std::function<int(int id)> getItemPrice
) {
    const float tW = 640.f, tH = 420.f;
    sf::FloatRect tBoxRect({ (width - tW) / 2.f, (height - tH) / 2.f }, { tW, tH });
    sf::RectangleShape tBox({ tW, tH });
    tBox.setPosition(tBoxRect.position);
    tBox.setFillColor(sf::Color(22, 22, 30, 248));
    tBox.setOutlineThickness(2.f);
    tBox.setOutlineColor(sf::Color(100, 180, 255));
    window.draw(tBox);

    // Header
    sf::RectangleShape header({ tW, 48.f });
    header.setPosition(tBoxRect.position);
    header.setFillColor(sf::Color(40, 60, 100));
    window.draw(header);
    sf::Text tTitle(uiFont, "TRADE WITH NPC", 26);
    tTitle.setStyle(sf::Text::Bold);
    tTitle.setFillColor(sf::Color(200, 230, 255));
    tTitle.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 8.f });
    window.draw(tTitle);

    // ─── PLAYER ITEMS section ───────────────────────────────
    sf::Text secLabel(uiFont, "Your Items  (click slot to add/remove)", 16);
    secLabel.setFillColor(sf::Color(160, 200, 255));
    secLabel.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 54.f });
    window.draw(secLabel);

    for (int i = 0; i < 4; i++) {
        sf::Vector2f slotPos = { tBoxRect.position.x + 20.f + i * 74.f, tBoxRect.position.y + 70.f };
        sf::RectangleShape slot({ 64.f, 64.f });
        slot.setPosition(slotPos);
        slot.setFillColor(playerTradeSlots[i].tex ? sf::Color(50, 80, 50) : sf::Color(50, 50, 60));
        slot.setOutlineThickness(2.f);
        slot.setOutlineColor(playerTradeSlots[i].tex ? sf::Color(80, 220, 80) : sf::Color(80, 80, 100));
        window.draw(slot);

        if (playerTradeSlots[i].tex) {
            sf::Sprite s(*playerTradeSlots[i].tex);
            sf::Vector2u sz = s.getTexture().getSize();
            float sc = 48.f / std::max(sz.x, sz.y);
            s.setScale({ sc, sc });
            s.setPosition({ slotPos.x + 8.f, slotPos.y + 8.f });
            window.draw(s);
            sf::Text ct(uiFont, "x" + std::to_string(playerTradeSlots[i].count), 14);
            ct.setFillColor(sf::Color::Yellow);
            ct.setPosition({ slotPos.x + 2.f, slotPos.y + 46.f });
            window.draw(ct);
        } else {
            sf::Text empty(uiFont, "+", 28);
            empty.setFillColor(sf::Color(80, 80, 100));
            empty.setPosition({ slotPos.x + 22.f, slotPos.y + 14.f });
            window.draw(empty);
        }
    }

    // ─── NPC ITEMS FOR SALE (buy mode — shown when no player items) ───
    if (!isSellingToNPC) {
        sf::Text npcSellLabel(uiFont, "NPC Items For Sale  (click to select)", 16);
        npcSellLabel.setFillColor(sf::Color(255, 220, 100));
        npcSellLabel.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 155.f });
        window.draw(npcSellLabel);

        int totalCost = 0;
        for (int i = 0; i < 4; i++) {
            if (npcTradeSlots[i].tex == nullptr) continue;
            sf::Vector2f slotPos = { tBoxRect.position.x + 20.f + i * 74.f, tBoxRect.position.y + 172.f };

            bool selected = npcTradeSlots[i].originalSlot == 1;
            sf::RectangleShape nSlot({ 64.f, 64.f });
            nSlot.setPosition(slotPos);
            nSlot.setFillColor(selected ? sf::Color(80, 60, 20) : sf::Color(60, 50, 30));
            nSlot.setOutlineThickness(2.f);
            nSlot.setOutlineColor(selected ? sf::Color(255, 200, 50) : sf::Color(140, 120, 60));
            window.draw(nSlot);

            sf::Sprite s(*npcTradeSlots[i].tex);
            sf::Vector2u sz = s.getTexture().getSize();
            float sc = 48.f / std::max(sz.x, sz.y);
            s.setScale({ sc, sc });
            s.setPosition({ slotPos.x + 8.f, slotPos.y + 8.f });
            window.draw(s);

            sf::Text ct(uiFont, "x" + std::to_string(npcTradeSlots[i].count), 14);
            ct.setFillColor(sf::Color::Yellow);
            ct.setPosition({ slotPos.x + 2.f, slotPos.y + 46.f });
            window.draw(ct);

            int itemPrice = getItemPrice(npcTradeSlots[i].id) * npcTradeSlots[i].count;
            itemPrice = (int)(itemPrice * 1.2f);
            sf::Text priceTxt(uiFont, std::to_string(itemPrice) + "G", 12);
            priceTxt.setFillColor(sf::Color(255, 220, 80));
            priceTxt.setPosition({ slotPos.x + 2.f, slotPos.y + 64.f });
            window.draw(priceTxt);

            if (selected) totalCost += itemPrice;
        }

        sf::Text costTxt(uiFont, "Selected Total: " + std::to_string(totalCost) + " G  |  You have: " + std::to_string(currency) + " G", 15);
        costTxt.setFillColor(totalCost <= currency ? sf::Color(100, 220, 100) : sf::Color(220, 80, 80));
        costTxt.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 252.f });
        window.draw(costTxt);
    }

    // ─── RIGHT PANEL: Gold fields ────────────────────────────
    float rx = tBoxRect.position.x + 360.f;
    float ry = tBoxRect.position.y + 60.f;

    // NPC Offer (read-only)
    sf::Text npcLabel(uiFont, "NPC Offer (Gold):", 16);
    npcLabel.setFillColor(sf::Color(200, 200, 200));
    npcLabel.setPosition({ rx, ry });
    window.draw(npcLabel);

    sf::RectangleShape nGoldBox({ 120.f, 34.f });
    nGoldBox.setPosition({ rx, ry + 22.f });
    nGoldBox.setFillColor(sf::Color(30, 30, 30));
    nGoldBox.setOutlineThickness(2.f);
    nGoldBox.setOutlineColor(sf::Color(120, 120, 120));
    window.draw(nGoldBox);
    sf::Text nGoldTxt(uiFont, npcGoldInput.empty() ? "0" : npcGoldInput, 18);
    nGoldTxt.setFillColor(sf::Color(100, 255, 100));
    nGoldTxt.setPosition({ rx + 8.f, ry + 26.f });
    window.draw(nGoldTxt);

    // Player Gold (editable)
    sf::Text pgLabel(uiFont, "Your Gold Offer:", 16);
    pgLabel.setFillColor(sf::Color(200, 200, 200));
    pgLabel.setPosition({ rx, ry + 68.f });
    window.draw(pgLabel);

    sf::RectangleShape pGoldBox({ 120.f, 34.f });
    pGoldBox.setPosition({ rx, ry + 90.f });
    pGoldBox.setFillColor(sf::Color(30, 30, 30));
    pGoldBox.setOutlineThickness(2.f);
    pGoldBox.setOutlineColor(playerGoldTyping ? sf::Color::Yellow : sf::Color(80, 120, 200));
    window.draw(pGoldBox);
    sf::Text pGoldTxt(uiFont, playerGoldInput.empty() ? "0" : playerGoldInput, 18);
    pGoldTxt.setFillColor(sf::Color::White);
    pGoldTxt.setPosition({ rx + 8.f, ry + 94.f });
    window.draw(pGoldTxt);

    // Stock Stats button
    sf::RectangleShape statsBtn({ 150.f, 36.f });
    statsBtn.setPosition({ rx, ry + 138.f });
    statsBtn.setFillColor(sf::Color(40, 80, 130));
    statsBtn.setOutlineThickness(1.f);
    statsBtn.setOutlineColor(sf::Color::White);
    window.draw(statsBtn);
    sf::Text statsTxt(uiFont, "Stock Stats", 16);
    statsTxt.setPosition({ rx + 20.f, ry + 147.f });
    window.draw(statsTxt);

    // ─── Status message ───────────────────────────────────────
    if (!bargainStatus.empty()) {
        sf::Text stTxt(uiFont, bargainStatus, 15);
        stTxt.setFillColor(bargainColor);
        stTxt.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 310.f });
        window.draw(stTxt);
    } else if (isSellingToNPC && npcTradeGold > 0) {
        sf::Text stTxt(uiFont, "NPC will pay " + std::to_string(npcTradeGold) + " Gold for your items.", 15);
        stTxt.setFillColor(sf::Color(100, 220, 100));
        stTxt.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 310.f });
        window.draw(stTxt);
    } else if (!isSellingToNPC) {
        float priceMul = bargainAccepted ? 1.08f : 1.20f;
        int totalCost = 0;
        for (int i = 0; i < 4; i++) {
            if (npcTradeSlots[i].originalSlot == 1 && npcTradeSlots[i].tex)
                totalCost += (int)(getItemPrice(npcTradeSlots[i].id) * npcTradeSlots[i].count * priceMul);
        }
        if (totalCost > 0) {
            std::string msg = "Cost: " + std::to_string(totalCost) + " G" + (bargainAccepted ? "  [10% OFF!]" : "");
            sf::Text stTxt(uiFont, msg, 15);
            stTxt.setFillColor(totalCost <= currency ? sf::Color(100, 220, 100) : sf::Color(220, 80, 80));
            stTxt.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 310.f });
            window.draw(stTxt);
        } else {
            sf::Text stTxt(uiFont, "Click NPC items below to select for purchase.", 15);
            stTxt.setFillColor(sf::Color(180, 180, 100));
            stTxt.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 310.f });
            window.draw(stTxt);
        }
    }

    // ─── Bottom buttons ───────────────────────────────────────
    float bY = tBoxRect.position.y + 355.f;

    sf::RectangleShape bargainBtn({ 100.f, 40.f });
    bargainBtn.setPosition({ tBoxRect.position.x + 20.f, bY });
    bool canBargain = false;
    if (isSellingToNPC) canBargain = npcTradeGold > 0 && !hasBargained && !bargainAccepted;
    else {
        for (int i = 0; i < 4; i++) if (npcTradeSlots[i].originalSlot == 1) canBargain = true;
        if (hasBargained || bargainAccepted) canBargain = false;
    }
    bargainBtn.setFillColor(canBargain ? sf::Color(200, 130, 30) : sf::Color(70, 70, 70));
    bargainBtn.setOutlineThickness(1.f);
    bargainBtn.setOutlineColor(sf::Color::White);
    window.draw(bargainBtn);
    sf::Text bargainTxt(uiFont, "Bargain", 16);
    bargainTxt.setPosition({ tBoxRect.position.x + 28.f, bY + 10.f });
    window.draw(bargainTxt);

    sf::RectangleShape cancelBtn({ 100.f, 40.f });
    cancelBtn.setPosition({ tBoxRect.position.x + 390.f, bY });
    cancelBtn.setFillColor(sf::Color(180, 50, 50));
    cancelBtn.setOutlineThickness(1.f);
    cancelBtn.setOutlineColor(sf::Color::White);
    window.draw(cancelBtn);
    sf::Text cancelTxt(uiFont, "Cancel", 16);
    cancelTxt.setPosition({ tBoxRect.position.x + 406.f, bY + 10.f });
    window.draw(cancelTxt);

    bool canAccept = false;
    if (isSellingToNPC) canAccept = npcTradeGold > 0;
    else { for (int i = 0; i < 4; i++) if (npcTradeSlots[i].originalSlot == 1 && npcTradeSlots[i].tex) canAccept = true; }
    sf::RectangleShape acceptBtn({ 100.f, 40.f });
    acceptBtn.setPosition({ tBoxRect.position.x + 500.f, bY });
    acceptBtn.setFillColor(canAccept ? sf::Color(40, 160, 70) : sf::Color(70, 70, 70));
    acceptBtn.setOutlineThickness(1.f);
    acceptBtn.setOutlineColor(sf::Color::White);
    window.draw(acceptBtn);
    sf::Text acceptTxt(uiFont, "Accept", 16);
    acceptTxt.setPosition({ tBoxRect.position.x + 516.f, bY + 10.f });
    window.draw(acceptTxt);

    sf::Text modeTxt(uiFont, isSellingToNPC ? "MODE: SELLING" : "MODE: BUYING", 14);
    modeTxt.setFillColor(isSellingToNPC ? sf::Color(100, 255, 100) : sf::Color(255, 200, 50));
    modeTxt.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 338.f });
    window.draw(modeTxt);
}

#include "Inventory.h"
#include "AuthManager.h"
#include "MarketStats.h"
#include "TextInput.h"
#include "ItemID.h"
#include <cmath>

bool handleP2PTradeClick(
    sf::Vector2f uiPos,
    sf::Vector2i mousePixelPos,
    unsigned int width,
    unsigned int height,
    int& p2pTradeID,
    const std::string& username,
    std::string& p2pPartner,
    TradeSlot myP2PSlots[4],
    int& myP2PGold,
    std::string& myP2PGoldInput,
    bool& p2pGoldTyping,
    bool& playerGoldTyping,
    bool& p2pMyReady,
    bool& p2pPartnerReady,
    bool& p2pOfferDirty,
    std::string& p2pStatusMsg,
    bool& p2pTradeOpen,
    std::vector<P2PItemDisplay>& partnerItems,
    int& partnerGold,
    Inventory& inventory,
    AuthManager& authManager,
    bool& quantityPromptOpen,
    int& quantityPromptMax,
    int& quantityPromptTargetSlot,
    int& quantityPromptSourceSlot,
    bool& quantityPromptIsP2P,
    TextInput& quantityInput,
    sf::RenderWindow& window
) {
    float tW = 700.f, tH = 440.f;
    sf::FloatRect tR({ (width - tW) / 2.f, (height - tH) / 2.f }, { tW, tH });

    // My item slots (left side, 4 slots)
    for (int i = 0; i < 4; i++) {
        sf::FloatRect sr({ tR.position.x + 20.f + i * 74.f, tR.position.y + 80.f }, { 64.f, 64.f });
        if (sr.contains(uiPos)) {
            if (myP2PSlots[i].tex) {
                inventory.addItem(myP2PSlots[i].id, *myP2PSlots[i].tex, myP2PSlots[i].count);
                myP2PSlots[i] = {};
            } else {
                int sel = inventory.getSelectedSlot();
                if (sel != -1 && inventory.hasItem(sel)) {
                    bool free = true;
                    for (int j = 0; j < 4; j++) if (myP2PSlots[j].originalSlot == sel) { free = false; break; }
                    if (free) {
                        int totalQty = inventory.getItemCount(sel);
                        if (totalQty > 1) {
                            quantityPromptOpen = true;
                            quantityPromptMax = totalQty;
                            quantityPromptTargetSlot = i;
                            quantityPromptSourceSlot = sel;
                            quantityPromptIsP2P = true;
                            quantityInput.clear();
                            quantityInput.setFocus(true);
                        } else {
                            myP2PSlots[i].id = inventory.getItemID(sel);
                            myP2PSlots[i].tex = inventory.getItemTexture(sel);
                            myP2PSlots[i].count = 1;
                            myP2PSlots[i].originalSlot = sel;
                            inventory.clearSlot(sel);
                            p2pOfferDirty = true;
                            p2pMyReady = false;
                        }
                    }
                }
            }
            return true;
        }
    }

    // My gold input
    sf::FloatRect goldBox({ tR.position.x + 20.f, tR.position.y + 165.f }, { 100.f, 34.f });
    if (goldBox.contains(uiPos)) {
        p2pGoldTyping = true;
        playerGoldTyping = false;
        return true;
    }

    // Ready button
    sf::FloatRect readyBtn({ tR.position.x + 20.f, tR.position.y + 380.f }, { 120.f, 40.f });
    if (readyBtn.contains(uiPos) && !p2pMyReady) {
        p2pMyReady = true;
        authManager.setP2PReady(p2pTradeID, username, true);
        std::string myCSV;
        for (int i = 0; i < 4; i++) {
            if (myP2PSlots[i].tex) {
                if (!myCSV.empty()) myCSV += ",";
                myCSV += std::to_string(myP2PSlots[i].id) + ":" + std::to_string(myP2PSlots[i].count);
            }
        }
        authManager.updateP2POffer(p2pTradeID, username, myCSV, myP2PGold);
        p2pStatusMsg = "You are ready! Waiting for " + p2pPartner + "...";
        return true;
    }

    // Unready button
    sf::FloatRect unreadyBtn({ tR.position.x + 155.f, tR.position.y + 380.f }, { 120.f, 40.f });
    if (unreadyBtn.contains(uiPos) && p2pMyReady) {
        p2pMyReady = false;
        authManager.setP2PReady(p2pTradeID, username, false);
        p2pStatusMsg = "Not ready. Adjust your offer.";
        return true;
    }

    // Cancel button
    sf::FloatRect cancelBtn({ tR.position.x + tW - 130.f, tR.position.y + 380.f }, { 120.f, 40.f });
    if (cancelBtn.contains(uiPos)) {
        authManager.cancelP2PTrade(p2pTradeID);
        for (int i = 0; i < 4; i++) {
            if (myP2PSlots[i].tex) {
                inventory.addItem(myP2PSlots[i].id, *myP2PSlots[i].tex, myP2PSlots[i].count);
                myP2PSlots[i] = {};
            }
        }
        p2pTradeOpen = false;
        p2pTradeID = -1;
        p2pPartner = "";
        p2pMyReady = false;
        p2pPartnerReady = false;
        partnerItems.clear();
        partnerGold = 0;
        return true;
    }

    // Fall through to inventory click
    inventory.handleClick(mousePixelPos, window);
    return true;
}

bool handleNPCTradeClick(
    sf::Vector2f uiPos,
    sf::Vector2i mousePixelPos,
    unsigned int width,
    unsigned int height,
    TradeSlot playerTradeSlots[4],
    TradeSlot npcTradeSlots[4],
    bool& isSellingToNPC,
    int& npcTradeGold,
    std::string& npcGoldInput,
    int& playerTradeGold,
    std::string& playerGoldInput,
    bool& playerGoldTyping,
    bool& stockPageOpen,
    bool& hasBargained,
    bool& bargainAccepted,
    std::string& bargainStatus,
    sf::Color& bargainColor,
    int& currency,
    bool& tradeMenuOpen,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    const std::string& username,
    float elapsedGameTime,
    int level,
    int exp,
    int hungerMultiplier,
    int thirstMultiplier,
    float hunger,
    float thirst,
    Inventory& inventory,
    AuthManager& authManager,
    std::vector<StockItem>& stockMarket,
    std::function<int(int id)> getItemPrice,
    std::function<std::string(int id)> getItemName,
    std::function<void(int id, float percentChange)> adjustMarketItem,
    bool& quantityPromptOpen,
    int& quantityPromptMax,
    int& quantityPromptTargetSlot,
    int& quantityPromptSourceSlot,
    bool& quantityPromptIsP2P,
    TextInput& quantityInput,
    sf::RenderWindow& window
) {
    sf::FloatRect tBoxRect({ (width - 640.f) / 2.f, (height - 420.f) / 2.f }, { 640.f, 420.f });

    // --- Player trade slot hit zones (4 slots) ---
    for (int i = 0; i < 4; i++) {
        sf::FloatRect slotRect({ tBoxRect.position.x + 20.f + i * 74.f, tBoxRect.position.y + 70.f }, { 64.f, 64.f });
        if (slotRect.contains(uiPos)) {
            if (playerTradeSlots[i].tex != nullptr) {
                inventory.addItem(playerTradeSlots[i].id, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
                playerTradeSlots[i] = {};
                bool anyLeft = false;
                for (int j = 0; j < 4; j++) if (playerTradeSlots[j].tex) anyLeft = true;
                if (!anyLeft) isSellingToNPC = false;
            } else {
                int selSlot = inventory.getSelectedSlot();
                if (selSlot != -1 && inventory.hasItem(selSlot)) {
                    bool slotFree = true;
                    for (int j = 0; j < 4; j++) {
                        if (playerTradeSlots[j].originalSlot == selSlot) { slotFree = false; break; }
                    }
                    if (slotFree) {
                        int totalQty = inventory.getItemCount(selSlot);
                        if (totalQty > 1) {
                            quantityPromptOpen = true;
                            quantityPromptMax = totalQty;
                            quantityPromptTargetSlot = i;
                            quantityPromptSourceSlot = selSlot;
                            quantityPromptIsP2P = false;
                            quantityInput.clear();
                            quantityInput.setFocus(true);
                        } else {
                            playerTradeSlots[i].id = inventory.getItemID(selSlot);
                            playerTradeSlots[i].tex = inventory.getItemTexture(selSlot);
                            playerTradeSlots[i].count = 1;
                            playerTradeSlots[i].originalSlot = selSlot;
                            inventory.clearSlot(selSlot);
                            isSellingToNPC = true;
                        }
                    }
                }
            }
            if (isSellingToNPC) {
                npcTradeGold = 0;
                for (int j = 0; j < 4; j++) {
                    if (playerTradeSlots[j].tex != nullptr) {
                        float demandMul = 0.75f;
                        for (auto& mkt : stockMarket) {
                            if (mkt.id == playerTradeSlots[j].id) {
                                demandMul = (mkt.trend == "UP") ? 0.90f : (mkt.trend == "DOWN") ? 0.60f : 0.75f;
                                break;
                            }
                        }
                        npcTradeGold += (int)(getItemPrice(playerTradeSlots[j].id) * demandMul) * playerTradeSlots[j].count;
                    }
                }
                npcGoldInput = std::to_string(npcTradeGold);
            }
            bargainAccepted = false;
            hasBargained = false;
            bargainStatus = "";
            bargainColor = sf::Color::White;
            return true;
        }
    }

    // NPC item slots for buy mode
    if (!isSellingToNPC) {
        for (int i = 0; i < 4; i++) {
            if (npcTradeSlots[i].tex == nullptr) continue;
            sf::FloatRect nSlotRect({ tBoxRect.position.x + 20.f + i * 74.f, tBoxRect.position.y + 172.f }, { 64.f, 64.f });
            if (nSlotRect.contains(uiPos)) {
                npcTradeSlots[i].originalSlot = (npcTradeSlots[i].originalSlot == 1) ? 0 : 1;
                bargainStatus = "";
                return true;
            }
        }
    }

    // Player Gold box
    sf::FloatRect playerGoldBox({ tBoxRect.position.x + 360.f, tBoxRect.position.y + 85.f }, { 120.f, 35.f });
    if (playerGoldBox.contains(uiPos)) {
        playerGoldTyping = true;
        return true;
    }

    // Stock stats
    sf::FloatRect statsBtnRect({ tBoxRect.position.x + 360.f, tBoxRect.position.y + 200.f }, { 150.f, 36.f });
    if (statsBtnRect.contains(uiPos)) {
        stockPageOpen = true;
        return true;
    }

    // Bargain
    sf::FloatRect bargainRect({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 355.f }, { 100.f, 40.f });
    if (bargainRect.contains(uiPos)) {
        if (isSellingToNPC && npcTradeGold > 0 && !hasBargained && !bargainAccepted) {
            if ((rand() % 100) < 40) {
                bargainAccepted = true;
                npcTradeGold = (int)(npcTradeGold * 1.15f);
                npcGoldInput = std::to_string(npcTradeGold);
                bargainStatus = "NPC: Fine! I'll add a little more.";
                bargainColor = sf::Color::Green;
            } else {
                hasBargained = true;
                bargainStatus = "NPC: No deal! Take it or leave it.";
                bargainColor = sf::Color::Red;
            }
            return true;
        } else if (!isSellingToNPC && !hasBargained && !bargainAccepted) {
            bool anySelected = false;
            for (int i = 0; i < 4; i++) if (npcTradeSlots[i].originalSlot == 1) anySelected = true;
            if (anySelected) {
                if ((rand() % 100) < 35) {
                    bargainAccepted = true;
                    bargainStatus = "NPC: Alright, 10% off just for you!";
                    bargainColor = sf::Color::Green;
                } else {
                    hasBargained = true;
                    bargainStatus = "NPC: Full price, take it or leave it!";
                    bargainColor = sf::Color::Red;
                }
                return true;
            }
        }
    }

    // Accept button
    sf::FloatRect acceptRect({ tBoxRect.position.x + 500.f, tBoxRect.position.y + 355.f }, { 100.f, 40.f });
    if (acceptRect.contains(uiPos)) {
        if (isSellingToNPC) {
            bool hasItems = false;
            for (int i = 0; i < 4; i++) if (playerTradeSlots[i].tex) hasItems = true;
            if (hasItems && npcTradeGold > 0) {
                currency += npcTradeGold;
                for (int i = 0; i < 4; i++) {
                    if (playerTradeSlots[i].tex) {
                        int item_id = playerTradeSlots[i].id;
                        int item_qty = playerTradeSlots[i].count;
                        std::string name = getItemName(item_id);
                        int uPrice = getItemPrice(item_id);
                        int totalVal = item_qty * uPrice;
                        float percentChange = -0.005f * item_qty;
                        adjustMarketItem(item_id, percentChange);
                        authManager.addTradeRecord(username.c_str(), "SELL", name, item_qty, uPrice, totalVal);
                        playerTradeSlots[i] = {};
                    }
                }
                latestPlayerMsg = "Sold! Received " + std::to_string(npcTradeGold) + " Gold.";
                bubbleTimer = 4.f;
                tradeMenuOpen = false;
                isSellingToNPC = false;
                npcTradeGold = 0; npcGoldInput = "0";
                playerTradeGold = 0; playerGoldInput = "0";
                bargainAccepted = false; hasBargained = false; bargainStatus = "";
                authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, elapsedGameTime, hungerMultiplier, thirstMultiplier);
            } else {
                bargainStatus = "Add items to your trade slots first!";
                bargainColor = sf::Color::Red;
            }
        } else {
            float priceMul = bargainAccepted ? 1.08f : 1.20f;
            int totalCost = 0;
            bool anySelected = false;
            for (int i = 0; i < 4; i++) {
                if (npcTradeSlots[i].originalSlot == 1 && npcTradeSlots[i].tex) {
                    anySelected = true;
                    totalCost += (int)(getItemPrice(npcTradeSlots[i].id) * npcTradeSlots[i].count * priceMul);
                }
            }
            if (!anySelected) {
                bargainStatus = "Select NPC items to buy first!";
                bargainColor = sf::Color::Red;
            } else if (totalCost > currency) {
                bargainStatus = "Not enough gold! Need " + std::to_string(totalCost) + " G.";
                bargainColor = sf::Color::Red;
            } else {
                currency -= totalCost;
                for (int i = 0; i < 4; i++) {
                    if (npcTradeSlots[i].originalSlot == 1 && npcTradeSlots[i].tex) {
                        inventory.addItem(npcTradeSlots[i].id, *npcTradeSlots[i].tex, npcTradeSlots[i].count);
                        int item_id = npcTradeSlots[i].id;
                        int item_qty = npcTradeSlots[i].count;
                        std::string name = getItemName(item_id);
                        int uPrice = getItemPrice(item_id);
                        int totalVal = item_qty * uPrice;
                        float percentChange = 0.005f * item_qty;
                        adjustMarketItem(item_id, percentChange);
                        authManager.addTradeRecord(username.c_str(), "BUY", name, item_qty, uPrice, totalVal);
                        npcTradeSlots[i] = {};
                    }
                }
                latestPlayerMsg = "Bought items for " + std::to_string(totalCost) + " Gold.";
                bubbleTimer = 4.f;
                tradeMenuOpen = false;
                isSellingToNPC = false;
                playerTradeGold = 0; playerGoldInput = "0";
                npcTradeGold = 0; npcGoldInput = "0";
                bargainAccepted = false; hasBargained = false; bargainStatus = "";
                authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, elapsedGameTime, hungerMultiplier, thirstMultiplier);
            }
        }
        return true;
    }

    // Cancel button
    sf::FloatRect cancelRect({ tBoxRect.position.x + 390.f, tBoxRect.position.y + 355.f }, { 100.f, 40.f });
    if (cancelRect.contains(uiPos)) {
        for (int i = 0; i < 4; i++) {
            if (playerTradeSlots[i].tex != nullptr) {
                inventory.addItem(playerTradeSlots[i].id, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
                playerTradeSlots[i] = {};
            }
            npcTradeSlots[i].originalSlot = 0;
        }
        tradeMenuOpen = false;
        isSellingToNPC = false;
        playerTradeGold = 0; playerGoldInput = "0";
        npcTradeGold = 0; npcGoldInput = "0";
        hasBargained = false; bargainAccepted = false; bargainStatus = "";
        bargainColor = sf::Color::White;
        return true;
    }

    inventory.handleClick(mousePixelPos, window);
    return true;
}

bool executeNPCTradeConfirm(
    bool confirmed,
    TradeSlot playerTradeSlots[4],
    TradeSlot npcTradeSlots[4],
    int& playerTradeGold,
    int& npcTradeGold,
    bool& hasBargained,
    bool& bargainAccepted,
    std::string& bargainStatus,
    sf::Color& bargainColor,
    int& currency,
    const std::string& username,
    float elapsedGameTime,
    int level,
    int exp,
    int hungerMultiplier,
    int thirstMultiplier,
    float hunger,
    float thirst,
    Inventory& inventory,
    AuthManager& authManager,
    std::vector<StockItem>& stockMarket
) {
    if (confirmed) {
        for (int i = 0; i < 4; i++) {
            if (playerTradeSlots[i].tex != nullptr && playerTradeSlots[i].count > 0) {
                std::string name = "";
                int price = 0;
                for (auto& item : stockMarket) {
                    if (item.id == playerTradeSlots[i].id) {
                        name = item.name;
                        item.currentPrice *= (1.0f - (bargainAccepted ? 0.008f : 0.005f) * playerTradeSlots[i].count);
                        float minP = item.basePrice * 0.4f;
                        float maxP = item.basePrice * 2.5f;
                        if (item.currentPrice < minP) item.currentPrice = minP;
                        if (item.currentPrice > maxP) item.currentPrice = maxP;
                        item.currentPrice = std::round(item.currentPrice);
                        if (item.currentPrice < 1.f) item.currentPrice = 1.f;
                        if (!item.history.empty()) {
                            float lastPrice = item.history.back();
                            float diff = item.currentPrice - lastPrice;
                            item.predictedPrice = std::round(item.currentPrice + diff * 0.5f);
                            if (item.predictedPrice < 1.f) item.predictedPrice = 1.f;
                            if (item.predictedPrice > item.currentPrice * 1.02f) item.trend = "UP";
                            else if (item.predictedPrice < item.currentPrice * 0.98f) item.trend = "DOWN";
                            else item.trend = "STABLE";
                        }
                        price = (int)item.currentPrice;
                        break;
                    }
                }
                authManager.addTradeRecord(username.c_str(), "SELL", name, playerTradeSlots[i].count, price, playerTradeSlots[i].count * price);
            }
        }

        for (int i = 0; i < 4; i++) {
            if (npcTradeSlots[i].tex != nullptr && npcTradeSlots[i].count > 0) {
                std::string name = "";
                int price = 0;
                for (auto& item : stockMarket) {
                    if (item.id == npcTradeSlots[i].id) {
                        name = item.name;
                        if (bargainAccepted) item.currentPrice *= 0.95f;
                        else item.currentPrice *= (1.0f + 0.008f * npcTradeSlots[i].count);
                        float minP = item.basePrice * 0.4f;
                        float maxP = item.basePrice * 2.5f;
                        if (item.currentPrice < minP) item.currentPrice = minP;
                        if (item.currentPrice > maxP) item.currentPrice = maxP;
                        item.currentPrice = std::round(item.currentPrice);
                        if (item.currentPrice < 1.f) item.currentPrice = 1.f;
                        if (!item.history.empty()) {
                            float lastPrice = item.history.back();
                            float diff = item.currentPrice - lastPrice;
                            item.predictedPrice = std::round(item.currentPrice + diff * 0.5f);
                            if (item.predictedPrice < 1.f) item.predictedPrice = 1.f;
                            if (item.predictedPrice > item.currentPrice * 1.02f) item.trend = "UP";
                            else if (item.predictedPrice < item.currentPrice * 0.98f) item.trend = "DOWN";
                            else item.trend = "STABLE";
                        }
                        price = (int)item.currentPrice;
                        break;
                    }
                }
                authManager.addTradeRecord(username.c_str(), "BUY", name, npcTradeSlots[i].count, price, npcTradeSlots[i].count * price);
            }
        }

        currency += playerTradeGold;
        currency -= npcTradeGold;
        playerTradeGold = 0;
        npcTradeGold = 0;
        hasBargained = false;
        bargainAccepted = false;
        bargainStatus = "";
        bargainColor = sf::Color::White;

        for (int i = 0; i < 4; i++) {
            if (npcTradeSlots[i].tex != nullptr && npcTradeSlots[i].count > 0) {
                inventory.addItem(npcTradeSlots[i].id, *npcTradeSlots[i].tex, npcTradeSlots[i].count);
                npcTradeSlots[i].tex = nullptr;
                npcTradeSlots[i].count = 0;
                npcTradeSlots[i].id = 0;
            }
            playerTradeSlots[i].tex = nullptr;
            playerTradeSlots[i].count = 0;
            playerTradeSlots[i].id = 0;
        }

        std::vector<std::pair<int, int>> items(20, { 0, 0 });
        for (int i = 0; i < 20; i++) {
            if (inventory.hasItem(i)) {
                items[i] = { inventory.getItemID(i), inventory.getItemCount(i) };
            }
        }
        authManager.saveInventory(username.c_str(), items);
        authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, elapsedGameTime, hungerMultiplier, thirstMultiplier);
    } else {
        for (int i = 0; i < 4; i++) {
            if (playerTradeSlots[i].tex != nullptr) {
                inventory.addItem(playerTradeSlots[i].id, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
                playerTradeSlots[i].tex = nullptr;
                playerTradeSlots[i].count = 0;
                playerTradeSlots[i].id = 0;
            }
            npcTradeSlots[i].tex = nullptr;
            npcTradeSlots[i].count = 0;
            npcTradeSlots[i].id = 0;
        }
        hasBargained = false;
        bargainAccepted = false;
        bargainStatus = "";
        bargainColor = sf::Color::White;
    }
    return true;
}

bool handleP2PInvitePopupClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    bool& p2pInvitePopup,
    int& p2pTradeID,
    bool& p2pTradeOpen,
    bool& p2pAmInitiator,
    std::string& p2pPartner,
    const P2PTradeState& p2pState,
    bool& p2pMyReady,
    bool& p2pPartnerReady,
    std::string& p2pStatusMsg,
    TradeSlot myP2PSlots[4],
    int& myP2PGold,
    std::string& myP2PGoldInput,
    std::vector<P2PItemDisplay>& partnerItems,
    int& partnerGold,
    AuthManager& authManager
) {
    float pw = 400.f, ph = 160.f;
    float px = (width - pw) / 2.f, py = (height - ph) / 2.f;
    sf::FloatRect acceptP({ px + 40.f, py + 100.f }, { 120.f, 40.f });
    sf::FloatRect declineP({ px + 240.f, py + 100.f }, { 120.f, 40.f });

    if (acceptP.contains(uiPos)) {
        authManager.acceptP2PTrade(p2pTradeID);
        p2pInvitePopup  = false;
        p2pTradeOpen    = true;
        p2pAmInitiator  = false;
        p2pPartner      = p2pState.initiator;
        p2pMyReady      = false;
        p2pPartnerReady = false;
        p2pStatusMsg    = "Trade open! Add items and gold, then press Ready.";
        for (int i = 0; i < 4; i++) myP2PSlots[i] = {};
        myP2PGold = 0; myP2PGoldInput = "0";
        partnerItems.clear(); partnerGold = 0;
        return true;
    }
    else if (declineP.contains(uiPos)) {
        authManager.cancelP2PTrade(p2pTradeID);
        p2pInvitePopup = false;
        p2pTradeID = -1;
        return true;
    }
    return false;
}

bool handleNPCTradeConfirmClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    bool& tradeConfirmOpen,
    bool& tradeMenuOpen,
    bool& tradeBoxesTicked,
    TradeSlot playerTradeSlots[4],
    TradeSlot npcTradeSlots[4],
    int& playerTradeGold,
    int& npcTradeGold,
    bool& hasBargained,
    bool& bargainAccepted,
    std::string& bargainStatus,
    sf::Color& bargainColor,
    int& currency,
    const std::string& username,
    float elapsedGameTime,
    int level,
    int exp,
    int hungerMultiplier,
    int thirstMultiplier,
    float hunger,
    float thirst,
    Inventory& inventory,
    AuthManager& authManager,
    std::vector<StockItem>& stockMarket
) {
    sf::FloatRect yesRect({ (width - 250.f) / 2.f, height / 2.f }, { 100.f, 40.f });
    sf::FloatRect noRect({ (width + 50.f) / 2.f, height / 2.f }, { 100.f, 40.f });

    if (yesRect.contains(uiPos)) {
        tradeConfirmOpen = false;
        tradeMenuOpen = false;
        tradeBoxesTicked = false;
        executeNPCTradeConfirm(
            true,
            playerTradeSlots,
            npcTradeSlots,
            playerTradeGold,
            npcTradeGold,
            hasBargained,
            bargainAccepted,
            bargainStatus,
            bargainColor,
            currency,
            username,
            elapsedGameTime,
            level,
            exp,
            hungerMultiplier,
            thirstMultiplier,
            hunger,
            thirst,
            inventory,
            authManager,
            stockMarket
        );
        return true;
    }
    else if (noRect.contains(uiPos)) {
        tradeConfirmOpen = false;
        tradeMenuOpen = false;
        tradeBoxesTicked = false;
        executeNPCTradeConfirm(
            false,
            playerTradeSlots,
            npcTradeSlots,
            playerTradeGold,
            npcTradeGold,
            hasBargained,
            bargainAccepted,
            bargainStatus,
            bargainColor,
            currency,
            username,
            elapsedGameTime,
            level,
            exp,
            hungerMultiplier,
            thirstMultiplier,
            hunger,
            thirst,
            inventory,
            authManager,
            stockMarket
        );
        return true;
    }
    return false;
}

#include <sstream>

void pollAndSyncP2PTrade(
    float dt,
    int& p2pTradeID,
    float& p2pPollTimer,
    float& p2pOfferPushTimer,
    P2PTradeState& p2pState,
    bool p2pAmInitiator,
    int& partnerGold,
    bool& p2pPartnerReady,
    bool& p2pMyReady,
    std::vector<P2PItemDisplay>& partnerItems,
    bool& p2pInvitePopup,
    bool& p2pTradeOpen,
    std::string& p2pStatusMsg,
    Inventory& inventory,
    std::function<const sf::Texture*(int id)> getItemTexture,
    std::function<std::string(int id)> getItemName,
    std::function<int(int id)> getItemPrice,
    std::function<void(int id, float delta)> adjustMarketItem,
    int& currency,
    TradeSlot myP2PSlots[4],
    int& myP2PGold,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    const std::string& username,
    std::string& p2pPartner,
    float hunger,
    float thirst,
    int level,
    int exp,
    float elapsedGameTime,
    int hungerMultiplier,
    int thirstMultiplier,
    bool& p2pOfferDirty,
    AuthManager& authManager
) {
    if (p2pTradeID <= 0) return;

    p2pPollTimer -= dt;
    p2pOfferPushTimer -= dt;
    if (p2pPollTimer <= 0.f) {
        p2pPollTimer = 1.0f;
        p2pState = authManager.getP2PTradeState(p2pTradeID);

        bool amI = p2pAmInitiator;
        std::string theirCSV = amI ? p2pState.targetItems : p2pState.initiatorItems;
        partnerGold = amI ? p2pState.targetGold : p2pState.initiatorGold;
        p2pPartnerReady = amI ? p2pState.targetReady : p2pState.initiatorReady;
        p2pMyReady      = amI ? p2pState.initiatorReady : p2pState.targetReady;

        partnerItems.clear();
        if (!theirCSV.empty()) {
            std::stringstream ss(theirCSV);
            std::string token;
            while (std::getline(ss, token, ',')) {
                auto colon = token.find(':');
                if (colon != std::string::npos) {
                    P2PItemDisplay d;
                    d.id    = std::stoi(token.substr(0, colon));
                    d.count = std::stoi(token.substr(colon + 1));
                    if (d.id > 0 && d.count > 0) partnerItems.push_back(d);
                }
            }
        }

        if (p2pState.status == "pending" && !p2pAmInitiator) {
            p2pInvitePopup = true;
            p2pTradeOpen   = false;
        }
        else if (p2pState.status == "accepted") {
            p2pTradeOpen   = true;
            p2pInvitePopup = false;
            if (p2pStatusMsg.find("Waiting") != std::string::npos)
                p2pStatusMsg = "Trade open! Add items and gold, then press Ready.";
        }
        else if (p2pState.status == "completed") {
            for (auto& d : partnerItems) {
                const sf::Texture* tex = getItemTexture(d.id);
                if (tex) inventory.addItem(d.id, *tex, d.count);
                if (adjustMarketItem) adjustMarketItem(d.id, 0.004f * d.count);

                std::string name = getItemName ? getItemName(d.id) : "";
                int uPrice = getItemPrice ? getItemPrice(d.id) : 0;
                int totalVal = d.count * uPrice;
                authManager.addTradeRecord(username.c_str(), "BUY", name, d.count, uPrice, totalVal);
            }
            currency += partnerGold;

            for (int i = 0; i < 4; i++) {
                if (myP2PSlots[i].tex) {
                    int item_id = myP2PSlots[i].id;
                    int item_qty = myP2PSlots[i].count;
                    std::string name = getItemName ? getItemName(item_id) : "";
                    int uPrice = getItemPrice ? getItemPrice(item_id) : 0;
                    int totalVal = item_qty * uPrice;
                    authManager.addTradeRecord(username.c_str(), "SELL", name, item_qty, uPrice, totalVal);
                }
            }

            for (int i = 0; i < 4; i++) myP2PSlots[i] = {};
            currency -= myP2PGold;
            myP2PGold = 0;
            latestPlayerMsg = "P2P trade completed with " + p2pPartner + "!";
            bubbleTimer = 4.f;
            authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, elapsedGameTime, hungerMultiplier, thirstMultiplier);
            p2pTradeOpen = false; p2pTradeID = -1; p2pPartner = "";
            p2pMyReady = false; p2pPartnerReady = false; p2pStatusMsg = "";
            partnerItems.clear(); partnerGold = 0;
        }
        else if (p2pState.status == "cancelled") {
            for (int i = 0; i < 4; i++) {
                if (myP2PSlots[i].tex) {
                    inventory.addItem(myP2PSlots[i].id, *myP2PSlots[i].tex, myP2PSlots[i].count);
                    myP2PSlots[i] = {};
                }
            }
            currency += myP2PGold; myP2PGold = 0;
            p2pTradeOpen = false; p2pInvitePopup = false; p2pTradeID = -1;
            p2pPartner = ""; p2pStatusMsg = "Trade cancelled.";
            latestPlayerMsg = "P2P trade cancelled."; bubbleTimer = 3.f;
        }

        if (p2pOfferDirty && p2pTradeID > 0 && p2pOfferPushTimer <= 0.f) {
            std::string myCSV;
            for (int i = 0; i < 4; i++) {
                if (myP2PSlots[i].tex) {
                    if (!myCSV.empty()) myCSV += ",";
                    myCSV += std::to_string(myP2PSlots[i].id) + ":" + std::to_string(myP2PSlots[i].count);
                }
            }
            authManager.updateP2POffer(p2pTradeID, username, myCSV, myP2PGold);
            for (int i = 0; i < 4; i++) {
                if (myP2PSlots[i].tex && adjustMarketItem) {
                    adjustMarketItem(myP2PSlots[i].id, -0.003f * myP2PSlots[i].count);
                }
            }
            p2pOfferDirty = false;
            p2pOfferPushTimer = 1.f;
        }

        if (p2pState.initiatorReady && p2pState.targetReady && p2pState.status == "accepted") {
            authManager.completeP2PTrade(p2pTradeID);
        }
    }
}
