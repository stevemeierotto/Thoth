// src/ChatMessagePanel.cpp
#include "ChatMessagePanel.h"
#include <memory>
#include <algorithm>
#include <wx/graphics.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/clipbrd.h>

#include <json.hpp>

ChatMessagePanel::ChatMessagePanel(wxWindow* parent, const wxString& message, bool isUser)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTRANSPARENT_WINDOW),
      m_message(message), m_isUser(isUser)
{
    // Safety: Limit massive messages that could crash the UI
    const size_t MAX_UI_MSG_SIZE = 50000;
    if (m_message.length() > MAX_UI_MSG_SIZE) {
        m_message = m_message.substr(0, MAX_UI_MSG_SIZE) + "\n... [TRUNCATED FOR UI PERFORMANCE] ...";
    }

    SetBackgroundStyle(wxBG_STYLE_PAINT);

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    // Check if the message is a JSON-encoded tool result
    bool isStructured = false;
    nlohmann::json toolJson;
    try {
        std::string stdMsg = m_message.ToStdString();
        if (!stdMsg.empty() && stdMsg[0] == '{') {
            toolJson = nlohmann::json::parse(stdMsg);
            if (toolJson.is_object() && toolJson.contains("status")) {
                isStructured = true;
            }
        }
    } catch (...) {}

    // Use a read-only text control to allow selection and copying
    m_textCtrl = new wxTextCtrl(this, wxID_ANY, m_message, wxDefaultPosition, wxDefaultSize,
                                 wxTE_MULTILINE | wxTE_READONLY | wxNO_BORDER | wxTE_RICH2);
    
    if (isStructured) {
        RenderStructuredContent(toolJson);
        m_textCtrl->Hide();
    } else {
        m_textCtrl->SetBackgroundColour(m_isUser ? wxColour(0, 120, 215) : wxColour(240, 240, 240));
        m_textCtrl->SetForegroundColour(m_isUser ? *wxWHITE : *wxBLACK);
        
        std::string stdMsg = m_message.ToStdString();
        int lineCount = static_cast<int>(std::count(stdMsg.begin(), stdMsg.end(), '\n')) + 1;
        if (m_message.Length() > 60 && lineCount == 1) {
            lineCount = static_cast<int>(m_message.Length() / 50) + 1;
        }
        int height = m_textCtrl->GetCharHeight() * (lineCount + 1) + 20;
        height = std::min(height, MAX_BUBBLE_HEIGHT);
        m_textCtrl->SetMinSize(wxSize(-1, height));
        m_textCtrl->SetMaxSize(wxSize(-1, MAX_BUBBLE_HEIGHT));
        sizer->Add(m_textCtrl, 0, wxALL | wxEXPAND, BUBBLE_PADDING + MARGIN);
    }

    // Optional: Add a small copy button
    wxButton* copyBtn = new wxButton(this, wxID_ANY, "Copy", wxDefaultPosition, wxDefaultSize);
    copyBtn->SetWindowStyle(wxBORDER_NONE);
    sizer->Add(copyBtn, 0, (m_isUser ? wxALIGN_RIGHT : wxALIGN_LEFT) | wxLEFT | wxRIGHT | wxBOTTOM, MARGIN);

    Bind(wxEVT_BUTTON, &ChatMessagePanel::OnCopy, this, copyBtn->GetId());
    
    SetSizer(sizer);
    Layout();
    
    // Final size including bubble padding
    SetMinSize(wxSize(-1, GetBestSize().y + MARGIN));
    
    Bind(wxEVT_PAINT, &ChatMessagePanel::OnPaint, this);
    Bind(wxEVT_SIZE, &ChatMessagePanel::OnSize, this);
}

void ChatMessagePanel::OnCopy(wxCommandEvent& WXUNUSED(event)) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(m_message));
        wxTheClipboard->Close();
        wxLogStatus("Message copied to clipboard.");
    }
}

void ChatMessagePanel::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(this);
    
    wxSize size = GetClientSize();
    if (size.x <= 0 || size.y <= 0) return;

    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc) return;

    // Find the text control among children
    wxRect bubbleRect = m_textCtrl->GetRect();
    bubbleRect.Inflate(BUBBLE_PADDING, BUBBLE_PADDING);

    // Modern flat colors
    wxColour bubbleColor = m_isUser ? wxColour(0, 120, 215) : wxColour(240, 240, 240);
    gc->SetBrush(wxBrush(bubbleColor));
    gc->SetPen(*wxTRANSPARENT_PEN);
    
    gc->DrawRoundedRectangle(bubbleRect.x, bubbleRect.y, bubbleRect.width, bubbleRect.height, BUBBLE_RADIUS);
}

void ChatMessagePanel::OnSize(wxSizeEvent& event) {
    Refresh();
    event.Skip();
}

void ChatMessagePanel::RenderStructuredContent(const nlohmann::json& j) {
    wxBoxSizer* sizer = static_cast<wxBoxSizer*>(GetSizer());
    if (!sizer) return;

    std::string status = j.value("status", "unknown");
    std::string toolName = j.value("tool_name", "Action");
    
    wxPanel* card = new wxPanel(this, wxID_ANY);
    card->SetBackgroundColour(status == "success" ? wxColour(232, 245, 233) : wxColour(255, 235, 238));
    wxBoxSizer* cardSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* header = new wxStaticText(card, wxID_ANY, "TOOL RESULT: " + toolName);
    header->SetFont(header->GetFont().Bold());
    cardSizer->Add(header, 0, wxALL, 5);

    if (j.contains("data")) {
        const auto& data = j["data"];
        if (toolName == "web_scrape" && data.contains("url")) {
            cardSizer->Add(new wxStaticText(card, wxID_ANY, "URL: " + data["url"].get<std::string>()), 0, wxLEFT|wxRIGHT, 10);
            if (data.contains("content")) {
                std::string content = data["content"].get<std::string>();
                if (content.size() > 300) content = content.substr(0, 297) + "...";
                wxStaticText* body = new wxStaticText(card, wxID_ANY, content);
                body->Wrap(400);
                cardSizer->Add(body, 0, wxALL, 10);
            }
        } else if (toolName == "summarize_text" && data.contains("summary")) {
            std::string summary = data["summary"].get<std::string>();
            if (summary.size() > 5000) summary = summary.substr(0, 4997) + "...";
            wxStaticText* body = new wxStaticText(card, wxID_ANY, summary);
            body->Wrap(400);
            cardSizer->Add(body, 0, wxALL, 10);
        }
 else if (toolName == "code_modify" && data.contains("diff")) {
            wxTextCtrl* diffBox = new wxTextCtrl(card, wxID_ANY, data["diff"].get<std::string>(), 
                                                 wxDefaultPosition, wxSize(400, 150), wxTE_MULTILINE | wxTE_READONLY);
            diffBox->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
            cardSizer->Add(diffBox, 1, wxEXPAND | wxALL, 5);
        } else {
            // Generic fallback - with safety limit
            std::string rawData = data.dump(2);
            if (rawData.size() > 5000) {
                rawData = rawData.substr(0, 4997) + "... [truncated]";
            }
            wxStaticText* body = new wxStaticText(card, wxID_ANY, rawData);
            body->Wrap(400);
            cardSizer->Add(body, 0, wxALL, 10);
        }
    }

    if (status != "success" && j.contains("error_message")) {
        wxStaticText* err = new wxStaticText(card, wxID_ANY, "Error: " + j["error_message"].get<std::string>());
        err->SetForegroundColour(*wxRED);
        cardSizer->Add(err, 0, wxALL, 5);
    }

    card->SetSizer(cardSizer);
    sizer->Insert(0, card, 0, wxALL | wxEXPAND, 5);
}
