// Project Rigs of Rods (http://www.rigsofrods.org)
//              ** SCRIPT EDITOR **
// Documentation: http://developer.rigsofrods.org
// ================================================

#include "imgui_utils.as" // ImHyperlink()

// Global definitions:
// -------------------

// from <imgui.h>
const int    ImGuiCol_Text = 0;
const int    ImGuiCol_TextDisabled = 1;
const int    ImGuiCol_WindowBg = 2;              // Background of normal windows
const int    ImGuiCol_ChildBg = 3;               // Background of child windows
const int    ImGuiCol_PopupBg = 4;               // Background of popups, menus, tooltips windows
// from <angelscript.h>
enum asEMsgType
{
	asMSGTYPE_ERROR       = 0,
	asMSGTYPE_WARNING     = 1,
	asMSGTYPE_INFORMATION = 2
};
// from rigs of rods
int SCRIPTUNITID_INVALID = -1;
const string RGN_SCRIPTS = "Scripts";
// others
const string BUFFER_TEXTINPUT_LABEL = "##bufferTextInputLbl";
const color LINKCOLOR = color(0.3, 0.5, 0.9, 1.0);
ScriptEditor editor;

// Config:
// -------

// LINE METAINFO COLUMNS
    // Line numbers
    bool drawLineNumbers=true;
    color lineNumberColor=color(1.f,1.f,0.7f,1.f);
    float lineNumColumnWidth = 25;
    // Line char counts        
    bool drawLineLengths=true;
    color lineLenColor=color(0.5f,0.4f,0.3f,1.f);
    float lineLenColumnWidth = 25;
    // Line comment offsets       
    bool drawLineCommentOffsets=true;
    color lineCommentOffsetColor=color(0.1f,0.9f,0.6f,0.6f);
    float lineCommentColumnWidth = 16;
    bool drawLineHttpOffsets=false; // only visual, links don't open
    color lineHttpOffsetColor=color(LINKCOLOR.r, LINKCOLOR.g, LINKCOLOR.a, 0.6f);
    float lineHttpColumnWidth = 16;        
    // Line err counts (error may span multiple AngelScript messages!)
    bool drawLineErrCounts = true;
    color lineErrCountColor = color(1.f,0.2f,0.1f,1.f);
    float errCountColumnWidth = 18;
// END line metainfo columns

// EDITOR settings
    color errorTextColor = color(1.f,0.3f,0.2f,1.f);
    color errorTextBgColor = color(0.1f,0.04f,0.08f,1.f);
    bool drawErrorText = true;
    color warnTextColor = color(0.78f,0.66f,0.22f,1.f);
    color warnTextBgColor = color(0.1f,0.08f,0.0f,1.f);
    bool drawWarnText = true;
    color commentHighlightColor = color(0.1f,0.9f,0.6f,0.16f);
    bool drawCommentHighlights = true;
    color httpHighlightColor=color(0.25, 0.3, 1, 0.3f);
    bool drawHttpHighlights = false; // only visual, links don't open    
    
    // input output
    bool saveShouldOverwrite=false;
// END editor

// Callback functions for the game:
// --------------------------------

void main() // Invoked by the game on startup
{
    game.log("Script editor started!");
    game.registerForEvent(SE_ANGELSCRIPT_MSGCALLBACK);
    game.registerForEvent(SE_ANGELSCRIPT_MANIPULATIONS);
    editor.setBuffer(TUT_SCRIPT);
}


void frameStep(float dt) // Invoked regularly by the game; the parameter is elapsed time in seconds.
{
    editor.draw();
}


void eventCallbackEx(scriptEvents ev, // Invoked by the game when a registered event is triggered
    int arg1, int arg2ex, int arg3ex, int arg4ex,
    string arg5ex, string arg6ex, string arg7ex, string arg8ex)
{
    if (ev == SE_ANGELSCRIPT_MSGCALLBACK)
    {
        editor.onEventAngelScriptMsg(
            /*id:*/arg1, /*eType:*/arg2ex, /*row:*/arg3ex, /*col:*/arg4ex, // ints
            /*sectionName:*/arg5ex, /*msg:*/arg6ex); // strings
    }
    else if (ev == SE_ANGELSCRIPT_MANIPULATIONS)
    {
        editor.onEventAngelScriptManip(
            /*manipType:*/arg1, /*nid:*/arg2ex, /*category:*/arg3ex, // ints
            /*name:*/arg5ex); // strings
    }
}

// The script editor logic:
// ------------------------

class ScriptEditor
{
    
    // THE TEXT BUFFER
        string buffer; // buffer data
        array<dictionary> bufferLinesMeta; // metadata for lines, see `analyzeLines()`
        array<array<uint>> bufferMessageIDs;
    // END text buffer
        
    // MESSAGES FROM ANGELSCRIPT ENGINE
        array<dictionary> messages;
        int messagesTotalErr = 0;
        int messagesTotalWarn = 0;
        int messagesTotalInfo = 0;
    // END messages from angelscript

    
    // CONTEXT
    float scrollY = 0.f;
    float scrollX = 0.f;
    float scrollMaxY = 0.f;
    float scrollMaxX = 0.f;
    float errCountOffset=0.f;
    vector2 errorsScreenCursor(0,0);
    vector2 errorsTextAreaSize(0,0);
    string bufferName = "(editor buffer)";
    int currentScriptUnitID = SCRIPTUNITID_INVALID;
    bool waitingForManipEvent = false; // When waiting for async result of (UN)LOAD_SCRIPT_REQUESTED
    string fileNameBuf;
    array<string> recentFileNames;
    array<dictionary> @localScriptsFileInfo = null;

    void refreshLocalFileList()
    {    
        const string RG_NAME = "Scripts";
        const string RG_PATTERN = "*.*";

        @this.localScriptsFileInfo = game.findResourceFileInfo(RG_NAME, RG_PATTERN);
    }    

    void draw()
    {
    
        int flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar;
        ImGui::Begin("Script editor", /*open:*/true, flags);
        
            this.drawMenubar();
            this.drawTextArea();
            this.drawFooter();         

            // To draw on top of editor text, we must trick DearIMGUI using an extra invisible window
            ImGui::SetCursorPos(errorsScreenCursor - ImGui::GetWindowPos());
            ImGui::PushStyleColor(ImGuiCol_ChildBg, color(0.f,0.f,0.f,0.f)); // Fully transparent background!
            int childFlags = ImGuiWindowFlags_NoInputs; // Necessary, otherwise editing is blocked.
            if (ImGui::BeginChild("ScriptEditorErr-child", errorsTextAreaSize,/*border:*/false, childFlags))
            {
                this.drawTextAreaErrors(errorsScreenCursor);   
            }
            ImGui::EndChild(); // must always be called - legacy reasons
            ImGui::PopStyleColor(1); // ChildBg  
        
        ImGui::End();

    }
    
    protected void drawMenubar()
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                //string loadTextResourceAsString(const std::string& filename, const std::string& resource_group);
                ImGui::InputText("File",/*inout:*/ fileNameBuf);
                if (ImGui::Button("Load"))
                {
                    
                    this.setBuffer(game.loadTextResourceAsString(fileNameBuf, RGN_SCRIPTS));
                    if (recentFileNames.find(fileNameBuf) < 0)
                        recentFileNames.insertLast(fileNameBuf);
                }
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 100.f);
                if (ImGui::Button("Save"))
                {
                    game.createTextResourceFromString(this.buffer, fileNameBuf, RGN_SCRIPTS, saveShouldOverwrite);
                    if (recentFileNames.find(fileNameBuf) < 0)
                        recentFileNames.insertLast(fileNameBuf);
                }
                ImGui::SameLine();
                ImGui::Checkbox("Overwrite", /*inout:*/saveShouldOverwrite);
                
                if (recentFileNames.length() > 0)
                {
                    ImGui::Separator();
                    ImGui::PushID("recentS");                    
                    ImGui::TextDisabled("Recent files:");
                    for (uint i = 0; i < recentFileNames.length(); i++)
                    {
                        ImGui::PushID(i);
                        ImGui::Bullet();
                        ImGui::SameLine(); ImGui::Text(recentFileNames[i]);
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Select"))
                        {
                            fileNameBuf = recentFileNames[i];
                        }
                        ImGui::PopID(); // i
                    }
                    ImGui::PopID(); // "recentS"
                }
                
                if (@this.localScriptsFileInfo != null)
                {
                    ImGui::Separator();
                    ImGui::PushID("localS");     
                    ImGui::TextDisabled("Local files ("+localScriptsFileInfo.length()+"):");
                    for (uint i=0; i<localScriptsFileInfo.length(); i++)
                    {
                        ImGui::PushID(i);
                        string filename = string(localScriptsFileInfo[i]['filename']);
                        uint size = uint(localScriptsFileInfo[i]['compressedSize']);
                        ImGui::Bullet(); 
                        ImGui::SameLine(); ImGui::Text(filename);
                        ImGui::SameLine(); ImGui::TextDisabled("("+float(size)/1000.f+" KB)");
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Select"))
                        {
                            fileNameBuf = filename;
                        }   
                        ImGui::PopID(); // i                        
                    }
                    ImGui::PopID(); // "localS"
                }   
                else
                {
                    ImGui::TextDisabled("localScriptsFileInfo is NULL");
                }                
                
                ImGui::EndMenu();
            }          
            if (ImGui::BeginMenu("View"))
            {
                ImGui::TextDisabled("Line info:");
                ImGui::PushStyleColor(ImGuiCol_Text, lineNumberColor);
                    ImGui::Checkbox("Line numbers", /*inout:*/drawLineNumbers);
                    ImGui::PopStyleColor(1); // ImGuiCol_Text
                ImGui::PushStyleColor(ImGuiCol_Text, lineLenColor);
                    ImGui::Checkbox("Char counts", /*inout:*/drawLineLengths);
                    ImGui::PopStyleColor(1); // ImGuiCol_Text
                ImGui::PushStyleColor(ImGuiCol_Text, lineCommentOffsetColor);
                    ImGui::Checkbox("Comment offsets", /*inout:*/drawLineCommentOffsets);
                    ImGui::PopStyleColor(1); // ImGuiCol_Text
                ImGui::PushStyleColor(ImGuiCol_Text, lineHttpOffsetColor);
                    ImGui::Checkbox("Http offsets", /*inout:*/drawLineHttpOffsets);
                    ImGui::PopStyleColor(1); // ImGuiCol_Text                    
                ImGui::PushStyleColor(ImGuiCol_Text, lineErrCountColor);
                    ImGui::Checkbox("Error counts", /*inout:*/drawLineErrCounts);
                    ImGui::PopStyleColor(1); // ImGuiCol_Text
                    
                ImGui::TextDisabled("Editor overlay:");
                ImGui::Checkbox("Error text", /*inout:*/drawErrorText);
                ImGui::Checkbox("Warning text", /*inout:*/drawWarnText);
                ImGui::Checkbox("Comments", /*inout:*/drawCommentHighlights);
                ImGui::Checkbox("Hyperlinks", /*inout:*/drawHttpHighlights);
                ImGui::EndMenu();
            }
            
            ImGui::Dummy(vector2(50, 1));
            if (this.waitingForManipEvent) // When waiting for async result of (UN)LOAD_SCRIPT_REQUESTED
            {
                ImGui::TextDisabled("WAIT...");
            }
            else
            {
                if (this.currentScriptUnitID == SCRIPTUNITID_INVALID)
                {            
                    if (ImGui::Button("[>>] RUN"))
                    {
                        this.runBuffer();
                    }
                }
                else
                {
                    if (ImGui::Button("[X] STOP"))
                    {
                        this.stopBuffer();
                    }
                }
            }
            
            ImGui::Dummy(vector2(50, 1));
            ImHyperlink('https://developer.rigsofrods.org/d2/d42/group___script_side_a_p_is.html', "Click for documentation");
            
            ImGui::EndMenuBar();
        }
    }
    
    protected void drawLineInfoColumns(vector2 screenCursor)
    {
        // draws columns with info (line number etc...) on the left side of the text area
        // ===============================================================================
        
        vector2 linenumCursorBase = screenCursor+vector2(0.f, -scrollY);
        ImDrawList@ drawlist = ImGui::GetWindowDrawList();
        int bufferNumLines = int(this.bufferLinesMeta.length());
        
        vector2 linenumCursor = linenumCursorBase;
        
        for (int linenum = 1; linenum <= bufferNumLines; linenum++)
        {
            int lineIdx = linenum - 1;
            if (drawLineNumbers)
            {
                drawlist.AddText(linenumCursor, lineNumberColor, ""+linenum);
                linenumCursor.x += lineNumColumnWidth;
            }
            
            if (drawLineLengths)
            {
                drawlist.AddText(linenumCursor, lineLenColor,
                    ""+int(this.bufferLinesMeta[lineIdx]['len']));
                linenumCursor.x += lineLenColumnWidth;                    
            }
            
            if (drawLineCommentOffsets)
            {
                drawlist.AddText(linenumCursor, lineCommentOffsetColor,
                    ""+int(this.bufferLinesMeta[lineIdx]['nonCommentLen']));
                linenumCursor.x += lineCommentColumnWidth;                    
            }          

            if (drawLineHttpOffsets)
            {
                drawlist.AddText(linenumCursor, lineHttpOffsetColor,
                    ""+int(this.bufferLinesMeta[lineIdx]['nonHttpLen']));
                linenumCursor.x += lineHttpColumnWidth;                    
            }     
            
            // draw message count
            if (drawLineErrCounts)
            {
                
                if (bufferMessageIDs.length() > lineIdx // sanity check
                    && bufferMessageIDs[lineIdx].length() > 0)
                {
                    // Draw num errors to the column

                    drawlist.AddText(linenumCursor, color(1.f,0.5f,0.4f,1.f), ""+bufferMessageIDs[lineIdx].length());        
                }
                linenumCursor.x += errCountColumnWidth;
            } 
            
            // prepare new Line
            linenumCursor.x = linenumCursorBase.x;
            linenumCursor.y += ImGui::GetTextLineHeight();
        }
    
           
    }
    
    protected void drawTextAreaErrors(vector2 screenCursor)
    {
        // draw messages to text area
        //===========================
        
        vector2 msgCursorBase = screenCursor + vector2(4, -this.scrollY);
        
        int bufferNumLines = int(this.bufferLinesMeta.length());
        ImDrawList@ drawlist = ImGui::GetWindowDrawList();
        
        for (int linenum = 1; linenum <= bufferNumLines; linenum++)
        {
            int lineIdx = linenum - 1;    
            vector2 lineCursor = msgCursorBase  + vector2(0.f, ImGui::GetTextLineHeight()*lineIdx);
            
            // sanity checks
            if (this.bufferLinesMeta.length() <= lineIdx)
                continue;
            
            // first draw comment highlights (semitransparent quads)
            int startOffset = int(this.bufferLinesMeta[lineIdx]['startOffset']);
            int lineLen = int(this.bufferLinesMeta[lineIdx]['len']);
            int nonCommentLen = int(this.bufferLinesMeta[lineIdx]['nonCommentLen']);
            if (lineLen != nonCommentLen && drawCommentHighlights)
            {
                // calc pos
                string nonCommentStr = buffer.substr(startOffset, nonCommentLen);
                vector2 nonCommentSize = ImGui::CalcTextSize(nonCommentStr);
                vector2 pos = lineCursor + vector2(nonCommentSize.x, 0.f);
                
                // calc size
                int doubleslashCommentStart = int(this.bufferLinesMeta[lineIdx]['doubleslashCommentStart']);
                string commentStr = buffer.substr(doubleslashCommentStart, lineLen-nonCommentLen);
                vector2 commentSize = ImGui::CalcTextSize(commentStr);
                
                drawlist.AddRectFilled(pos, pos+commentSize, commentHighlightColor);
            }
            
            // second - http highlights
            int httpStart = int(this.bufferLinesMeta[lineIdx]['httpStart']);
            int httpLen = int(this.bufferLinesMeta[lineIdx]['httpLen']);
            if (httpStart >= 0 && httpLen >= 0 && drawHttpHighlights)
            {
                // calc pos
                int nonHttpLen = httpStart - startOffset;
                string nonHttpStr = buffer.substr(startOffset, nonHttpLen);
                vector2 nonHttpSize = ImGui::CalcTextSize(nonHttpStr);
                vector2 pos = lineCursor + vector2(nonHttpSize.x, 0.f);
            
                // calc size
                string httpStr = buffer.substr(httpStart, httpLen);
                vector2 httpSize = ImGui::CalcTextSize(httpStr);
                
                drawlist.AddRectFilled(pos, pos+httpSize, httpHighlightColor);
                
                // underline
                // FIXME: draw underline when hyperlink behaviours work here
                //        - The overlay window is 'NoInputs' so buttons don't work.
                //        - The base window doesn't receive clicks for some reason.
                //drawlist.AddLine(pos+vector2(0,httpSize.y), pos+httpSize, LINKCOLOR);                
            }
            
            // sanity check
            if (this.bufferMessageIDs.length() <= lineIdx)
                continue;
            
            // then errors and warnings
            for (uint i = 0; i < this.bufferMessageIDs[lineIdx].length(); i++)
            {
                uint msgIdx = this.bufferMessageIDs[lineIdx][i];
        
                int msgRow = int(this.messages[msgIdx]['row']);
                int msgCol = int(this.messages[msgIdx]['col']);
                int msgType = int(this.messages[msgIdx]['type']);
                string msgText = string(this.messages[msgIdx]['message']);
                
                color col, bgCol;
                bool shouldDraw = false;
                switch (msgType)
                {
                    case asMSGTYPE_ERROR:
                        col = errorTextColor;
                        bgCol = errorTextBgColor;
                        shouldDraw = drawErrorText;
                        break;
                    case asMSGTYPE_WARNING:
                        col = warnTextColor;
                        bgCol = warnTextBgColor;
                        shouldDraw = drawWarnText;
                        break;
                    default:;
                }
                
                if (!shouldDraw)
                    continue;
                
                // Calc horizontal offset
                string subStr = buffer.substr(startOffset, msgCol-1);
                vector2 textPos = lineCursor + vector2(
                    ImGui::CalcTextSize(subStr).x, // X offset - under the indicated text position
                    ImGui::GetTextLineHeight()-2); // Y offset - slightly below the current line                
  
                // Draw message text with background
                string errText = "^ "+msgText;
                vector2 errTextSize = ImGui::CalcTextSize(errText);
                drawlist.AddRectFilled(textPos, textPos+errTextSize, bgCol);
                drawlist.AddText(textPos , col, errText);
                
                // advance to next line
                lineCursor.y += ImGui::GetTextLineHeight();
            }
        }            
            

    }
    
    protected void drawTextArea()
    {
        
        // Reserve space for the metainfo columns
        float metaColumnsTotalWidth = 0.f;
        if (drawLineNumbers)
            metaColumnsTotalWidth += lineNumColumnWidth;
        if (drawLineLengths)
            metaColumnsTotalWidth += lineLenColumnWidth;
        if (drawLineCommentOffsets)
            metaColumnsTotalWidth += lineCommentColumnWidth;      
        if (drawLineHttpOffsets)
            metaColumnsTotalWidth += lineHttpColumnWidth;  
        errCountOffset = metaColumnsTotalWidth;
        if (drawLineErrCounts)
            metaColumnsTotalWidth += errCountColumnWidth;
           
        // Draw the multiline text input
        vector2 textAreaSize(
            ImGui::GetWindowSize().x - (metaColumnsTotalWidth + 20), 
            ImGui::GetWindowSize().y - 90);
        vector2 screenCursor = ImGui::GetCursorScreenPos() + /*frame padding:*/vector2(0.f, 4.f);         
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + metaColumnsTotalWidth);            
        bool changed = ImGui::InputTextMultiline(BUFFER_TEXTINPUT_LABEL, this.buffer, textAreaSize);

        
        // Trick: re-enter the inputtext panel to scroll it; see https://github.com/ocornut/imgui/issues/1523
        ImGui::BeginChild(ImGui::GetID(BUFFER_TEXTINPUT_LABEL));
        this.scrollX = ImGui::GetScrollX();
        this.scrollY = ImGui::GetScrollY();
        this.scrollMaxX = ImGui::GetScrollMaxX();
        this.scrollMaxY = ImGui::GetScrollMaxY();
        ImGui::EndChild();
        
        if (changed)
        {
            this.analyzeLines();
        }        
        
        this.drawLineInfoColumns(screenCursor);
        screenCursor.x+= metaColumnsTotalWidth;
        // Apparently drawing errors now makes them appear under the editor text;
        // we must remember values and draw separately.
        errorsScreenCursor = screenCursor;
        errorsTextAreaSize = textAreaSize;
        
        // make top gap (window padding) rougly same size as bottom gap (item spacing)
        ImGui::SetCursorPosY(ImGui::GetCursorPosY()+2); 
    }
    
    protected void drawFooter()
    {
        // footer with status info
        ImGui::Separator();
        string runningTxt = "NOT RUNNING";
        if (this.currentScriptUnitID != SCRIPTUNITID_INVALID)
        {
            runningTxt = "RUNNING (NID: "+this.currentScriptUnitID+")";
        }
        ImGui::Text(runningTxt
            +"; lines: "+bufferLinesMeta.length()
            +"; messages:"+this.messages.length()
                +"(err:"+this.messagesTotalErr
                +"/warn:"+this.messagesTotalWarn
                +"/info:"+this.messagesTotalInfo
            +") scroll: X="+scrollX+"/"+scrollMaxX
                +"; Y="+scrollY+"/"+scrollMaxY+"");    
    }    
    
    void setBuffer(string data)
    {
        this.buffer = data;
        this.analyzeLines();    
        this.refreshLocalFileList();        
    }
    
    void runBuffer()
    {
        this.bufferMessageIDs.resize(0); // clear all
        this.messages.resize(0); // clear all
        
        
        game.pushMessage(MSG_APP_LOAD_SCRIPT_REQUESTED, {
            {'filename', this.bufferName}, // Because we supply the buffer, this will serve only as display name
            {'buffer', this.buffer },
            {'category', SCRIPT_CATEGORY_CUSTOM}
        });
        waitingForManipEvent=true;
    }
    
    void stopBuffer()
    {
        game.pushMessage(MSG_APP_UNLOAD_SCRIPT_REQUESTED, {
            {'id', this.currentScriptUnitID}
        });
        waitingForManipEvent=true;
    }    
    
    void onEventAngelScriptMsg(int scriptUnitId, int msgType, int row, int col, string sectionName, string message)
    {
        /*game.log("DBG '"+this.bufferName+"' onEventAngelScriptMsg(): scriptUnitId:" + scriptUnitId
            + ", msgType:" + msgType + ", row:" + row + ", col:" + col // ints
            + ", sectionName:" + sectionName + ", message:" + message); // strings*/
            
        messages.insertLast({ {'type', msgType}, {'row',row}, {'col', col}, {'sectionName', sectionName}, {'message', message} });
        this.analyzeMessages();
    }
    
    void onEventAngelScriptManip(int manipType, int scriptUnitId, int scriptCategory, string scriptName)
    {
        /*game.log ("DBG '"+this.bufferName+"'.onEventAngelScriptManip(): manipType:"+ manipType+", scriptUnitId:"+ scriptUnitId
            + ", scriptCategory:"+scriptCategory+", scriptName:"+scriptName+"; //this.waitingForManipEvent:"+this.waitingForManipEvent);*/
        
        if (this.waitingForManipEvent)
        {        
            if (manipType == ASMANIP_SCRIPT_LOADED
                && this.currentScriptUnitID == SCRIPTUNITID_INVALID
                && this.bufferName == scriptName)
            {
                this.currentScriptUnitID = scriptUnitId;
                waitingForManipEvent = false;
                /*game.log ("DBG '"+this.bufferName+"'.onEventAngelScriptManip(): Now running with NID="+this.currentScriptUnitID);*/
            }
            else if (manipType == ASMANIP_SCRIPT_UNLOADED
                && this.currentScriptUnitID != SCRIPTUNITID_INVALID
                && this.bufferName == scriptName)
            {
                /*game.log ("DBG '"+this.bufferName+"'.onEventAngelScriptManip(): Stopping, was using NID="+this.currentScriptUnitID);*/
                this.currentScriptUnitID = SCRIPTUNITID_INVALID;
                waitingForManipEvent = false;
            }
        }
    }
    
    void analyzeLines()
    {
        this.analyzeBuffer();
        this.analyzeMessages();
    }
    
    private void analyzeBuffer() // helper for `analyzeLines()`
    {
        this.bufferLinesMeta.resize(0); // clear all
        int startOffset = 0;
        this.bufferLinesMeta.insertLast({ {'startOffset', startOffset} });
        
        // pattern - comment
        string commentPattern = "//";
        uint commentLevel = 0;
        int commentStart = -1;
        bool commentFound = false;
        
        // pattern - hyperlink
        string httpPattern = 'http://';
        uint httpLevel = 0;
        int httpStart = -1;
        bool httpFound = false;
        
        string httpBreakChars = " \n\t\"'";
        bool httpBreakFound = false;
        int httpBreakPos = -1;
        
        for (uint i = 0; i < this.buffer.length(); i++)
        {
            uint lineIdx = this.bufferLinesMeta.length() - 1;
        
            // doubleslash comment
            if (!commentFound)
            {
                if (this.buffer[i] == commentPattern[commentLevel]
                    && (httpStart == -1 || httpBreakFound)) // Either we're not in hyperlink yet or we're after it already.)
                {
                    commentLevel++;
                    
                    // Record the '//', but make sure it's not within hyperlink!
                    if (commentStart == -1)
                    {
                        commentStart = i;
                    }
                    
                    if (uint(commentLevel) == commentPattern.length())
                    {
                        commentFound = true;
                    }                    
                }
                else
                {
                    commentLevel = 0;
                }
            }
            
            // http protocol
            if (!httpFound)
            {
                if (this.buffer[i] == httpPattern[httpLevel])
                {
                    httpLevel++;
                    
                    if (httpStart == -1)
                    {
                        httpStart = i;
                    }
                    
                    if (uint(httpLevel) >= httpPattern.length())
                    {
                        httpFound = true;
                    }                    
                }
                else
                {
                    httpLevel = 0;
                }
            }
            else if (!httpBreakFound)
            {
                for (uint j = 0; j < httpBreakChars.length(); j++)
                {
                    if (this.buffer[i] == httpBreakChars[j])
                    {
                        httpBreakFound = true;
                        httpBreakPos = i;
                    }
                }
            }
        
            if (this.buffer[i] == uint(0x0a)) // ASCII newline
            {
                // Finish line
                
                int endOffset = i;
                this.bufferLinesMeta[lineIdx]['endOffset'] = endOffset;
                int len = endOffset - startOffset;
                this.bufferLinesMeta[lineIdx]['len'] = len;
                this.bufferLinesMeta[lineIdx]['doubleslashCommentStart'] = commentStart;
                int nonCommentLen = (commentStart >= 0) ? commentStart - startOffset : len;
                this.bufferLinesMeta[lineIdx]['nonCommentLen'] = nonCommentLen;
                this.bufferLinesMeta[lineIdx]['httpStart'] = (httpFound) ? httpStart : -1;
                this.bufferLinesMeta[lineIdx]['httpLen'] = (httpFound) ? httpBreakPos-httpStart : -1;
                this.bufferLinesMeta[lineIdx]['nonHttpLen'] = (httpFound) ? httpStart - startOffset : -1;
                
                // reset context
                commentFound = false;
                commentStart = -1; // -1 means Empty
                commentLevel = 0;
                httpFound = false;
                httpStart = -1; // -1 means Empty    
                httpLevel = 0;
                httpBreakFound = false;
                httpBreakPos = -1; // -1 means Empty
                
                // Start new line
                startOffset = i+1;
                this.bufferLinesMeta.insertLast({ {'startOffset', startOffset} });
            }
        }
    }
 
    private void analyzeMessages() // helper for `analyzeLines()`
    {
        // reset caches
        bufferMessageIDs.resize(0); // clear all
        bufferMessageIDs.resize(bufferLinesMeta.length());
    
        // reset global stats
        this.messagesTotalErr = 0;
        this.messagesTotalWarn = 0;
        this.messagesTotalInfo = 0;
    
        
        for (uint i = 0; i < this.messages.length(); i++)
        {
            int msgRow = int(this.messages[i]['row']);
            int msgType = int(this.messages[i]['type']);
            string msgText = string(this.messages[i]['message']);
            int lineIdx = msgRow-1;
            
            // update line stats
            if (msgType == asMSGTYPE_ERROR)
            {
                this.bufferLinesMeta[lineIdx]['messagesNumErr'] = int(this.bufferLinesMeta[lineIdx]['messagesNumErr'])+1;
                this.bufferMessageIDs[lineIdx].insertLast(i);
                this.messagesTotalErr++;
            }
            if (msgType == asMSGTYPE_WARNING)
            {
                this.bufferLinesMeta[lineIdx]['messagesNumWarn'] = int(this.bufferLinesMeta[lineIdx]['messagesNumWarn'])+1;
                this.bufferMessageIDs[lineIdx].insertLast(i);
                this.messagesTotalWarn++;
            }
            if (msgType == asMSGTYPE_INFORMATION)
            {
                this.bufferLinesMeta[lineIdx]['messagesNumInfo'] = int(this.bufferLinesMeta[lineIdx]['messagesNumInfo'])+1;
                this.messagesTotalInfo++;
            }
        }
        
    }
 
}

// Tutorial scripts 
// -----------------
// using 'heredoc' syntax; see https://www.angelcode.com/angelscript/sdk/docs/manual/doc_datatypes_strings.html

const string TUT_SCRIPT =
"""
// TUTORIAL SCRIPT - Shows the basics, step by step:
// how to store data and update/draw them every frame.
// ===================================================

// total time in seconds
float tt = 0.f;

// `frameStep()` runs every frame; `dt` is delta time in seconds.
void frameStep(float dt)
{
    // accumulate time
    tt += dt;
    
    // format the output 
    string ttStr = "Total time: " + formatFloat(tt, "", 5, 2) + "sec";
    string dtStr = "Delta time: " + formatFloat(dt, "", 7, 4) + "sec";
    
    // draw the output to implicit window titled "Debug"
    ImGui::Text(ttStr);
    ImGui::Text(dtStr);
}
""";