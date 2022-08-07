#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <unordered_map>
#include <stack>
#include <deque>
#include <unordered_set>
using namespace std;


// 符号表条目
struct entry{
    bool isConst;
    int number;
    string name;
    int level;
    entry(){
        isConst = false;
        level = 911;
    }
};


// 全局变量
static int tempVarCount = 0; // 所有临时变量以数字命名，依次递增。根据这个变量获取上一个运算得到的临时变量名。
static int blockCount = 0; // 块号也类似。不过小心“同步”问题。
static bool haveBlock = true; // 要特别小心基本块的匹配问题，一定以ret、br、jump之一结尾，且不能为空。用这个全局布尔变量标记当前基本块是否结束
static bool isBlockEnd = false;

static unordered_map<string,entry> tempSymbolTable;
static deque<unordered_map<string,entry> > deq(1,tempSymbolTable); // 专门用于初始化符号表栈的两个全局静态变量，使这个栈一开始就压入一个符号表，可以用于记录全局变量的信息
static stack<unordered_map<string,entry> > symbolTableStack(deq); // 符号表栈，解决局部变量的作用域问题。
static unordered_set<string> symbolSet; // 判断每一个koopa中的变量名字是否被用过。
static stack<int> continueStack; // 为了给continue语句记录下跳转到的基本块标号而设立。栈方便解决多重循环嵌套。
static stack<int> breakStack;    // 同上
static unordered_map<string, bool> isFuncVoid;

// 声明
class UnaryExpAST;
class MulExpAST;
class AddExpAST;
class LOrExpAST;
class PrimaryExpAST;
class ConstExpAST;
class FuncDefAST;

static entry searchSymbolTable(string);




// 所有 AST 的基类
class BaseAST {
public:
    virtual ~BaseAST() = default;
    virtual void Dump()  = 0;
    virtual int valueSpread(){ return 0;}
};


// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST {
public:
    // 用智能指针管理对象
    unique_ptr<BaseAST> func_defs;

    void Dump() {
        func_defs->Dump();
    }
};


class FuncTypeAST : public BaseAST{
public:
    int type; //"int" is 0, "void" is 1

    FuncTypeAST(){
        type = 0;
    }
    void Dump() {
        if(type == 0){
            cout << ": i32 ";
        } 
    }
};


// FuncDef 也是 BaseAST
// FuncDef   ::= FuncType IDENT "(" ")" Block;
class FuncDefAST : public BaseAST {
public:
    //unique_ptr<BaseAST> func_type;
    BaseAST* func_type;
    string ident;
    unique_ptr<BaseAST> block;

    void Dump() {
        cout << "fun ";
        cout << "@" << ident << "()";
        func_type->Dump();
        
        bool isVoid = dynamic_cast<FuncTypeAST*>(func_type)->type; // !
        isFuncVoid.emplace(ident, isVoid);

        cout << "{" << endl;
        cout << "%entry:" << endl;

        haveBlock = true;
        block->Dump();

        if(isVoid){
            if(haveBlock){
                cout << "   ret" << endl;
            }else{
                cout << "%block_" << blockCount << ":" << endl;
                blockCount++;
                cout << "   ret" << endl;
            }
        }
        
        cout << "}" << endl;
    }
};


class FuncDefinesAST : public BaseAST{
public:
    vector<BaseAST*> funcdefList;

    void Dump(){
        for(auto i : funcdefList){
            dynamic_cast<FuncDefAST*>(i)->Dump();
        }
    }
};




class BlockAST : public BaseAST{
public:
    unique_ptr<BaseAST> items;

    void Dump() {
        unordered_map<string,entry> st;
        symbolTableStack.push(st);

        //cout << "{" << endl;
        //cout << "%entry:" << endl;
        items->Dump();
        //cout << "}" << endl;

        symbolTableStack.pop();
    }
};


// Stmt      ::= "return" Exp ";"; | ...
class StmtAST : public BaseAST{
public:
    bool isReturn;
    int condition = 0;
    unique_ptr<BaseAST> exp;
    unique_ptr<BaseAST> block;
    string id;
    unique_ptr<BaseAST> ifstmt;
    
    void Dump(){
        if(!haveBlock ){
            cout << "%block_" << blockCount << ":" << endl;
            blockCount++;
            haveBlock = true;
        }
        if(condition == 1){// return ;
            cout << "   ret" << endl;
            isBlockEnd = true;
            haveBlock = false;
        }else if(condition == 2){ // block
            block->Dump();
        }else if(condition == 3){// exp;
            exp->Dump();
        }else if(condition == 4){
            // do nothing
        }else if(condition == 5){ // WHILE '(' exp ')' IfStmt
            // 一定是有block标号的，但是不一定有语句。
            int b0 = blockCount, b1 = b0 + 1, b2 = b0 + 2;
            blockCount += 3;
            cout << "   jump %block_" << b0 << endl;
            cout << "%block_" << b0 << ":" << endl;
            exp->Dump();
            cout << "   br %" << tempVarCount - 1 << ", %block_" << b1 << ", %block_" << b2 << endl;
            cout << "%block_" << b1 << ":" << endl;
            continueStack.push(b0); breakStack.push(b2);
            ifstmt->Dump();

            if(!haveBlock ){
                cout << "%block_" << blockCount << ":" << endl;
                blockCount++;
                haveBlock = true;
            }

            cout << "   jump %block_" << b0 << endl;
            cout << "%block_" << b2 << ":" << endl;
            continueStack.pop(); breakStack.pop();
        }else if(condition == 6){ // continue;
            cout << "   jump %block_" << continueStack.top() << endl;
            haveBlock = false;
        }else if(condition == 7){ // break;
            cout << "   jump %block_" << breakStack.top() << endl;
            haveBlock = false;
        }else if(isReturn){
            exp->Dump();
            cout << "   ret %" << tempVarCount-1;
            cout << endl;
            isBlockEnd = true;
            haveBlock = false;
        }else{// lval = exp;
            exp->Dump();
            entry e = searchSymbolTable(id);
            string koopaid = id + "__" + to_string(e.level);
            cout << "   store %" << tempVarCount - 1 << ", @" << koopaid << endl;
        }
    }
};


class ExpAST : public BaseAST{
public:
    unique_ptr<BaseAST> lorexp;

    void Dump() {
        lorexp->Dump();
    }
    int valueSpread(){
        return (lorexp)->valueSpread();
    }
};


// UnaryExp      ::= PrimaryExp | UnaryOp UnaryExp | ...
class UnaryExpAST : public BaseAST{
public:
    int varNumber;
    unique_ptr<BaseAST> primaryexp;
    vector<char> unaryopList;
    string callName;

    void Dump() {
        if(callName == ""){
            primaryexp->Dump();
        }else{ // function call
            bool isVoid = isFuncVoid[callName];
            if(isVoid){
                cout << "   call @" << callName << "()" << endl;
            }else{
                cout << "   %" << tempVarCount << " = call @" << callName << "()" << endl;
                tempVarCount++;
            }
        }
        
        for(char c : unaryopList){
            if(c == '!'){
                cout << "   %" << tempVarCount << " = eq 0, %" << (tempVarCount-1) << endl;
                tempVarCount++;
            }else if(c == '+'){
                // do nothing
            }else if(c == '-'){
                cout << "   %" << tempVarCount << " = sub 0, %" << (tempVarCount-1) << endl;
                tempVarCount++;
            }
        }
        varNumber = tempVarCount - 1;
    }
    int valueSpread(){
        int ans = (primaryexp)->valueSpread();
        for(char c : unaryopList){
            if(c == '!'){
                ans = !ans;
            }else if(c == '-'){
                ans = -ans;
            }
        }
        return ans;
    }
};


// MulExp        ::= UnaryExp | MulExp ("*" | "/" | "%") UnaryExp;
class MulExpAST : public BaseAST{
public:
    int varNumber;
    vector<char> opList;
    vector<BaseAST*> unaryexpList;

    void Dump() {
        dynamic_cast<UnaryExpAST*>(unaryexpList[0])->Dump();
        varNumber = tempVarCount - 1;
        for(int i=0;i<opList.size();i++){
            unaryexpList[i+1]->Dump();
            int tr = dynamic_cast<UnaryExpAST*>(unaryexpList[i+1])->varNumber;
            if(opList[i] == '*'){
                cout << "   %" << tempVarCount << " = mul %" << varNumber << ", %" << tr << endl;
                tempVarCount++;
            }else if(opList[i] == '/'){
                cout << "   %" << tempVarCount << " = div %" << varNumber << ", %" << tr << endl;
                tempVarCount++;
            }else if(opList[i] == '%'){
                cout << "   %" << tempVarCount << " = mod %" << varNumber << ", %" << tr << endl;
                tempVarCount++;
            }
            varNumber = tempVarCount - 1;
        }
    }
    int valueSpread(){
        int ans = dynamic_cast<UnaryExpAST*>(unaryexpList[0])->valueSpread();
        for(int i=1;i<unaryexpList.size();i++){
            int t = dynamic_cast<UnaryExpAST*>(unaryexpList[i])->valueSpread();
            if(opList[i-1] == '*'){
                ans = ans * t;
            }else if(opList[i-1] == '/'){
                ans = ans / t;
            }else
                ans = ans % t;
        }
        return ans;
    }
};


// AddExp        ::= MulExp | AddExp ("+" | "-") MulExp;
class AddExpAST : public BaseAST{
public:
    int varNumber;
    vector<char> opList;
    vector<BaseAST*> mulexpList;

    void Dump(){
        dynamic_cast<MulExpAST*>(mulexpList[0])->Dump();
        varNumber = tempVarCount - 1;
        for(int i=0;i<opList.size();i++){
            mulexpList[i+1]->Dump();
            int tr = dynamic_cast<MulExpAST*>(mulexpList[i+1])->varNumber;
            if(opList[i] == '+'){
                cout << "   %" << tempVarCount << " = add %" << varNumber << ", %" << tr << endl;
                tempVarCount++;
            }else if(opList[i] == '-'){
                cout << "   %" << tempVarCount << " = sub %" << varNumber << ", %" << tr << endl;
                tempVarCount++;
            }
            varNumber = tempVarCount - 1;
        }
    }
    int valueSpread(){
        int ans = dynamic_cast<MulExpAST*>(mulexpList[0])->valueSpread();
        for(int i=1;i<mulexpList.size();i++){
            int t = dynamic_cast<MulExpAST*>(mulexpList[i])->valueSpread();
            if(opList[i-1] == '+'){
                ans = ans + t;
            }else{
                ans = ans - t;
            }
        }
        return ans;
    }
};


// PrimaryExp    ::= "(" Exp ")" | LVal | Number;
class PrimaryExpAST : public BaseAST{
public:
    bool isNum;
    bool isVar;
    int number;
    unique_ptr<BaseAST> exp;
    string id;
//
    void Dump(){
        if(isNum){
            cout << "   %" << tempVarCount << " = add 0, " << number << endl;
            tempVarCount++;
        }else if(isVar){
            entry e = searchSymbolTable(id);
            if(!e.isConst){
                string koopaid = id + "__" + to_string(e.level);
                cout << "   %" << tempVarCount << " = load @" << koopaid << endl;
                tempVarCount++;
            }else{
                number = e.number;
                cout << "   %" << tempVarCount << " = add 0, " << number << endl;
                tempVarCount++;
            }
            
        }else{ // (exp)
            exp->Dump();
        }
    }
    int valueSpread(){
        if(isNum){
            return number;
        }else if(isVar){
            entry e = searchSymbolTable(id);
            return e.number;
        }else{
            return (exp)->valueSpread();
        }
    }
};


class UnaryOpAST : public BaseAST{
public:
    char unaryop;
    UnaryOpAST(char c){
        unaryop = c;
    }
    void Dump(){
        //cout << unaryop;
    }
};


class NumberAST : public BaseAST{
public:
    int num;
    NumberAST(){}
    NumberAST(int n){
        num = n;
    }
    void Dump() {
        //cout << num;
    }
};


//RelExp      ::= AddExp | RelExp ("<" | ">" | "<=" | ">=") AddExp;
class RelExpAST : public BaseAST{
public:
    int varNumber;
    vector<BaseAST*> addexpList;
    vector<char> opList;

    void Dump(){
        dynamic_cast<AddExpAST*>(addexpList[0])->Dump();
        varNumber = tempVarCount - 1;
        for(int i=0;i<opList.size();i++){
            addexpList[i+1]->Dump();
            int tr = dynamic_cast<AddExpAST*>(addexpList[i+1])->varNumber;
            if(opList[i] == '>'){
                cout << "   %" << tempVarCount << " = gt %" << varNumber << ", %" << tr << endl;
                tempVarCount++;
            }else if(opList[i] == '<'){
                cout << "   %" << tempVarCount << " = lt %" << varNumber << ", %" << tr << endl;
                tempVarCount++;
            }else if(opList[i] == ','){ // "<="
                cout << "   %" << tempVarCount << " = le %" << varNumber << ", %" << tr << endl;
                tempVarCount++;
            }else if(opList[i] == '.'){ // ">="
                cout << "   %" << tempVarCount << " = ge %" << varNumber << ", %" << tr << endl;
                tempVarCount++;
            }
            varNumber = tempVarCount - 1;
        }
    }
    int valueSpread(){
        int ans = dynamic_cast<AddExpAST*>(addexpList[0])->valueSpread();
        for(int i=1;i<addexpList.size();i++){
            int t = dynamic_cast<AddExpAST*>(addexpList[i])->valueSpread();
            if(opList[i-1] == '>'){
                ans = (ans > t);
            }else if(opList[i-1] == '<'){
                ans = (ans < t);
            }else if(opList[i-1] == ','){
                ans = (ans <= t);
            }else{
                ans = (ans >= t);
            }
        }
        return ans;
    }
};


// EqExp       ::= RelExp | EqExp ("==" | "!=") RelExp;
class EqExpAST : public BaseAST{
public:
    int varNumber;
    vector<BaseAST*> relexpList;
    vector<bool> opList;

    void Dump(){
        dynamic_cast<RelExpAST*>(relexpList[0])->Dump();
        varNumber = tempVarCount - 1;
        for(int i=0;i<opList.size();i++){
            relexpList[i+1]->Dump();
            int tr = dynamic_cast<RelExpAST*>(relexpList[i+1])->varNumber;
            if(opList[i] == true){
                cout << "   %" << tempVarCount << " = eq %" << varNumber << ", %" << tr << endl;
                tempVarCount++;
            }else if(opList[i] == false){
                cout << "   %" << tempVarCount << " = ne %" << varNumber << ", %" << tr << endl;
                tempVarCount++;
            }
            varNumber = tempVarCount - 1;
        }
    }
    int valueSpread(){
        int ans = dynamic_cast<RelExpAST*>(relexpList[0])->valueSpread();
        for(int i=1;i<relexpList.size();i++){
            int t = dynamic_cast<RelExpAST*>(relexpList[i])->valueSpread();
            if(opList[i-1])
                ans = (ans == t);
            else
                ans = (ans != t);
        }
        return ans;
    }
};


// LAndExp     ::= EqExp | LAndExp "&&" EqExp;
class LAndExpAST : public BaseAST{
public:
    int varNumber;
    vector<BaseAST*> eqexpList;

    void Dump(){
        dynamic_cast<EqExpAST*>(eqexpList[0])->Dump();
        varNumber = tempVarCount - 1;
        for(int i=1;i<eqexpList.size();i++){
            eqexpList[i]->Dump();
            int tr = dynamic_cast<EqExpAST*>(eqexpList[i])->varNumber;

            cout << "   %" << tempVarCount << " = ne 0, %" << varNumber << endl;
            tempVarCount++;
            cout << "   %" << tempVarCount << " = ne 0, %" << tr << endl;
            tempVarCount++;
            cout << "   %" << tempVarCount << " = and %" << tempVarCount - 2 << ", %" << tempVarCount - 1 << endl;
            tempVarCount++;
            varNumber = tempVarCount - 1;
        }
    }
    int valueSpread(){
        int ans = dynamic_cast<EqExpAST*>(eqexpList[0])->valueSpread();
        for(int i=1;i<eqexpList.size();i++){
            int t = dynamic_cast<EqExpAST*>(eqexpList[i])->valueSpread();
            ans = ans && t;
        }
        return ans;
    }
};


// LOrExp      ::= LAndExp | LOrExp "||" LAndExp;
class LOrExpAST : public BaseAST{
public:
    int varNumber;
    vector<BaseAST*> landexpList;

    void Dump(){
        dynamic_cast<LAndExpAST*>(landexpList[0])->Dump();
        varNumber = tempVarCount - 1;
        for(int i=1;i<landexpList.size();i++){
            landexpList[i]->Dump();
            int tr = dynamic_cast<LAndExpAST*>(landexpList[i])->varNumber;

            cout << "   %" << tempVarCount << " = ne 0, %" << varNumber << endl;
            tempVarCount++;
            cout << "   %" << tempVarCount << " = ne 0, %" << tr << endl;
            tempVarCount++;
            cout << "   %" << tempVarCount << " = or %" << tempVarCount - 2 << ", %" << tempVarCount - 1 << endl;
            tempVarCount++;
            varNumber = tempVarCount - 1;
        }
    }
    int valueSpread(){
        int ans = dynamic_cast<LAndExpAST*>(landexpList[0])->valueSpread();
        for(int i=1;i<landexpList.size();i++){
            int t = dynamic_cast<LAndExpAST*>(landexpList[i])->valueSpread();
            ans = ans || t;
        }
        return ans;
    }
};



class ItemsAST : public BaseAST{
public:
    vector<BaseAST*> itemsList;

    void Dump(){
        for(auto i : itemsList){
            i->Dump();
        }
    }
};


// BlockItem     ::= Decl | Stmt;
class BlockItemAST : public BaseAST{
public:
    bool isDecl;
    unique_ptr<BaseAST> stmt;
    unique_ptr<BaseAST> decl;
    
    void Dump(){
        if(isDecl){
            decl->Dump();
        }else{
            stmt->Dump();
        }
    }
};


// Decl          ::= ConstDecl | VarDecl ;
class DeclAST : public BaseAST{
public:
    unique_ptr<BaseAST> constDecl;
    unique_ptr<BaseAST> varDecl;
    bool isConst;

    void Dump(){
        if(!isConst){
            varDecl->Dump();
        }else{
            constDecl->Dump();
        }
    }
};


// ConstDecl     ::= "const" BType ConstDef {"," ConstDef} ";";
class ConstDeclAST : public BaseAST{
public:
    //vector<BaseAST> constdefList;
    unique_ptr<BaseAST> constDefines;

    void Dump(){
        constDefines->Dump();
    }
};


class ConstDefinesAST : public BaseAST{
public:
    vector<BaseAST*> constdefList;

    void Dump(){
        for(auto i : constdefList){
            i->Dump();
        }
    }
};


class ConstDefAST : public BaseAST{
public:
    string id;
    int value;
    unique_ptr<BaseAST> constInitial;

    void Dump(){
        value = constInitial->valueSpread();
        struct entry e;
        e.isConst = true;
        e.level = symbolTableStack.size();
        e.name = id;
        e.number = value;

        unordered_map<string,entry> st = symbolTableStack.top();
        st.emplace(id, e);
        symbolTableStack.pop();
        symbolTableStack.push(st);

    }
};


class ConstExpAST : public BaseAST{
public:
    unique_ptr<BaseAST> exp;

    void Dump(){

    }
    int valueSpread(){
        return (exp)->valueSpread();
    }
};


class ConstInitialAST : public BaseAST{
public:
    unique_ptr<BaseAST> constExp;

    void Dump(){

    }
    int valueSpread(){
        return (constExp)->valueSpread();
    }
};


class InitialAST : public BaseAST{
public:
    unique_ptr<BaseAST> exp;

    void Dump(){
        exp->Dump();
    }
};


class VarDefAST : public BaseAST{
public:
    bool isInitial;
    unique_ptr<BaseAST> initial;
    string id;
    int level;
    

    void Dump(){
        
        level = symbolTableStack.size();
        entry e;
        e.isConst = false; e.name = id;e.level = symbolTableStack.size();
        unordered_map<string,entry> st = symbolTableStack.top();
        st.emplace(id,e);
        symbolTableStack.pop();
        symbolTableStack.push(st);
        

        string koopaid = id + "__" + to_string(level);
        int isKoopaidUsed = symbolSet.count(koopaid);
        if(isInitial){
            initial->Dump();
            if(isKoopaidUsed == 0){
                cout << "   @" << koopaid << " = alloc i32" << endl;
                symbolSet.emplace(koopaid);
            }
            cout << "   store %" << tempVarCount - 1 << ", @" << koopaid << endl;
        }else{
            if(isKoopaidUsed == 0){
                cout << "   @" << koopaid << " = alloc i32" << endl;
                symbolSet.emplace(koopaid);
            }
        }
    }
};


class VarDefinesAST : public BaseAST{
public:
    vector<BaseAST*> vardefList;

    void Dump(){
        for(auto i : vardefList){
            i->Dump();
        }
    }
};


class VarDeclAST : public BaseAST{
public:
    unique_ptr<BaseAST> varDefines;

    void Dump(){
        if(!haveBlock){
            cout << "%block_" << blockCount << ":" << endl;
            blockCount++;
            haveBlock = true;
        }
        isBlockEnd = false;

        varDefines->Dump();
    }
};


class MatchedStmtAST : public BaseAST{
public:
    bool isIf;
    unique_ptr<BaseAST> stmt;
    unique_ptr<BaseAST> exp;
    unique_ptr<BaseAST> thenstmt;
    unique_ptr<BaseAST> elsestmt;

    void Dump(){
        if(!isIf){
            stmt->Dump();
        }else{ // IF '(' Exp ')' Matched_stmt ELSE Matched_stmt
            exp->Dump();
            int b1 = blockCount, b2 = b1 + 1, b3 = b1 + 2;
            blockCount += 3;
            cout << "   br %" << tempVarCount - 1 << ", %block_" << b1 << ", %block_" << b2 << endl;
            cout << "%block_" << b1 << ":" << endl;
            haveBlock = true;
            thenstmt->Dump();
            bool flag1 = isBlockEnd;
            if(!flag1){
                if(!haveBlock ){
                    cout << "%block_" << blockCount << ":" << endl;
                    blockCount++;
                    haveBlock = true;
                }
                cout << "   jump %block_" << b3 << endl;
            }
            
            cout << "%block_" << b2 << ":" << endl;
            haveBlock = true;
            elsestmt->Dump();
            bool flag2 = isBlockEnd;
            if(!flag2){
                if(!haveBlock ){
                    cout << "%block_" << blockCount << ":" << endl;
                    blockCount++;
                    haveBlock = true;
                }
                cout << "   jump %block_" << b3 << endl;
            }
            
            haveBlock = false;
            if(!flag1 || !flag2){
                cout << "%block_" << b3 << ":" << endl;
                haveBlock = true;
            }

        }
    }
};


class OpenStmtAST : public BaseAST{
public:
    bool isElse;
    unique_ptr<BaseAST> exp;
    unique_ptr<BaseAST> thenstmt;
    unique_ptr<BaseAST> elsestmt;

    void Dump(){
        if(isElse){ // IF '(' Exp ')' Matched_stmt ELSE Open_stmt
            exp->Dump();
            int b1 = blockCount, b2 = b1 + 1, b3 = b1 + 2;
            blockCount += 3;
            cout << "   br %" << tempVarCount - 1 << ", %block_" << b1 << ", %block_" << b2 << endl;
            cout << "%block_" << b1 << ":" << endl;
            haveBlock = true;
            thenstmt->Dump();
            bool flag1 = isBlockEnd;
            if(!flag1){
                if(!haveBlock ){
                    cout << "%block_" << blockCount << ":" << endl;
                    blockCount++;
                    haveBlock = true;
                }
                cout << "   jump %block_" << b3 << endl;
            }
            
            cout << "%block_" << b2 << ":" << endl;
            haveBlock = true;
            elsestmt->Dump();
            bool flag2 = isBlockEnd;
            if(!flag2){
                if(!haveBlock ){
                    cout << "%block_" << blockCount << ":" << endl;
                    blockCount++;
                    haveBlock = true;
                }
                cout << "   jump %block_" << b3 << endl;
            }
            
            haveBlock = false;
            if(!flag1 || !flag2){
                cout << "%block_" << b3 << ":" << endl;
                haveBlock = true;
            }
        }else{ // IF '(' Exp ')' IfStmt
            exp->Dump();
            int b0 = blockCount, b1 = b0 + 1;
            blockCount += 2;
            cout << "   br %" << tempVarCount - 1 << ", %block_" << b0 << ", %block_" << b1 << endl;
            cout << "%block_" << b0 << ":" << endl;
            haveBlock = true;
            thenstmt->Dump();
            if(!isBlockEnd){
                if(!haveBlock ){
                    cout << "%block_" << blockCount << ":" << endl;
                    blockCount++;
                    haveBlock = true;
                }
                cout << "   jump %block_" << b1 << endl;
            }
            cout << "%block_" << b1 << ":" << endl;
            haveBlock = true;
            
        }
    }
};


class IfStmtAST : public BaseAST{
public:
    bool isMatched;
    unique_ptr<BaseAST> stmt;

    void Dump(){
        if(!haveBlock){
            cout << "%block_" << blockCount << ":" << endl;
            blockCount++;
            haveBlock = true;
        }
        isBlockEnd = false;

        stmt->Dump();
    }
};







static entry searchSymbolTable(string name){
    //cout << "hello?" << endl;
    unordered_map<string,entry>::iterator it;
    unordered_map<string,entry> st;
    stack<unordered_map<string,entry> > garbageSatck;
    entry e;
    bool isFind = false;
    while(!symbolTableStack.empty()){
      st = symbolTableStack.top();
      it = st.find(name);
      if(it != st.end()){
        e = it->second;
        isFind = true;
        break;
      }
      garbageSatck.push(st);
      symbolTableStack.pop();
    }

    if(!isFind){
        e.isConst = false;
        e.level = 404;
        e.name = "not found";
    }

    while(!garbageSatck.empty()){
      unordered_map<string, entry> temp = garbageSatck.top();
      garbageSatck.pop();
      symbolTableStack.push(temp);
    }
    return e;
}




