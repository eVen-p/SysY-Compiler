%code requires {
  #include <memory>
  #include <string>
  #include "ast.hpp"
}

%{

#include <iostream>
#include <memory>
#include <string>
#include "ast.hpp"

// 声明 lexer 函数和错误处理函数
int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

extern stack<unordered_map<string,entry> > symbolTableStack;

%}

// 定义 parser 函数和错误处理函数的附加参数
%parse-param { std::unique_ptr<BaseAST> &ast }

// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 因为 token 的值有的是字符串指针, 有的是整数
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 至于为什么要用字符串指针而不直接用 string 或者 unique_ptr<string>?
// 请自行 STFW 在 union 里写一个带析构函数的类会出现什么情况
%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
}

// lexer 返回的所有 token 种类的声明
// 注意 IDENT 和 INT_CONST 会返回 token 的值, 分别对应 str_val 和 int_val
%token INT RETURN AND OR EQUAL NEQUAL GEQUAL LEQUAL CONST IF ELSE WHILE CONTINUE BREAK VOID
%token <str_val> IDENT
%token <int_val> INT_CONST

// 非终结符的类型定义
%type <ast_val> FuncDef FuncType Block Stmt Number UnaryOp Exp UnaryExp PrimaryExp AddExp MulExp RelExp EqExp LAndExp LOrExp
%type <ast_val> BlockItem Items Decl ConstDecl ConstDef ConstInitial ConstExp ConstDefines Initial VarDecl VarDef VarDefines
%type <ast_val> IfStmt Matched_stmt Open_stmt FuncDefines
%type <str_val> LVal

%%

// 开始符, CompUnit ::= FuncDef, 大括号后声明了解析完成后 parser 要做的事情
// 而 parser 一旦解析完 CompUnit, 就说明所有的 token 都被解析了, 即解析结束了
// 此时我们应该把 FuncDef 返回的结果收集起来, 作为 AST 传给调用 parser 的函数
// $1 指代规则里第一个符号的返回值, 也就是 FuncDef 的返回值

CompUnit
  : FuncDefines {
    auto comp_unit = make_unique<CompUnitAST>();
    comp_unit->func_defs = unique_ptr<BaseAST>($1);
    ast = move(comp_unit);
  }
  ;

FuncDefines 
  : FuncDef{
    auto f = new FuncDefinesAST();
    f->funcdefList.push_back($1);
    $$ = f;
  }
  | FuncDefines FuncDef{
    auto f = dynamic_cast<FuncDefinesAST*>($1);
    f->funcdefList.push_back($2);
    $$ = f;
  }
  ;

// FuncDef ::= FuncType IDENT '(' ')' Block;
// 我们这里可以直接写 '(' 和 ')', 因为之前在 lexer 里已经处理了单个字符的情况
// 解析完成后, 把这些符号的结果收集起来, 然后拼成一个新的BaseAST, 作为结果返回
// $$ 表示非终结符的返回值, 我们可以通过给这个符号赋值的方法来返回结果
// 你可能会问, FuncType, IDENT 之类的结果已经是**类了
// 为什么还要用 unique_ptr 接住它们, 然后再解引用, 把它们拼成另一个字符串指针呢
// 因为所有的指针都是我们 new 出来的, new 出来的内存一定要 delete
// 否则会发生内存泄漏, 而 unique_ptr 这种智能指针可以自动帮我们 delete
// 虽然此处你看不出用 unique_ptr 和手动 delete 的区别, 但当我们定义了 AST 之后
// 这种写法会省下很多内存管理的负担
FuncDef
  : FuncType IDENT '(' ')' Block {
    auto func_ast = new FuncDefAST();
    //func_ast->func_type = unique_ptr<BaseAST>($1);
    func_ast->func_type = ($1);
    func_ast->ident = *unique_ptr<string>($2);
    func_ast->block = unique_ptr<BaseAST>($5);
    $$ = func_ast;
  }
  ;

// 同上, 不再解释
FuncType
  : INT {
    //cout << "FuncType" << endl;
    //cout << symbolTableStack.size() << endl;
    auto ft = new FuncTypeAST();
    $$ = ft;
  }
  | VOID {
    auto ft = new FuncTypeAST();
    ft->type = 1;
    $$ = ft;
  }
  ;

Block
  : '{' Items '}' {
    auto b = new BlockAST();
    b->items = unique_ptr<BaseAST>($2);
    $$ = b;
  }
  ;

Items
  : BlockItem{
    auto s = new ItemsAST();
    s->itemsList.push_back($1);
    $$ = s;
  }
  | Items BlockItem{
    auto ptr = dynamic_cast<ItemsAST*>($1);
    ptr->itemsList.push_back($2);
    $$ = ptr;
  }
  | {
    auto s = new ItemsAST();
    $$ = s;
  }
  ;

BlockItem
  : Decl{
    auto bi = new BlockItemAST();
    bi->isDecl = true;
    bi->decl = unique_ptr<BaseAST>($1);
    $$ = bi;
  }
  | IfStmt{
    auto bi = new BlockItemAST();
    bi->isDecl = false;
    bi->stmt = unique_ptr<BaseAST>($1);
    $$ = bi;
  }
  ;

Decl
  : ConstDecl{
    auto s = new DeclAST();
    s->constDecl = unique_ptr<BaseAST>($1);
    s->isConst = true;
    $$ = s;
  }
  | VarDecl{
    auto s = new DeclAST();
    s->varDecl = unique_ptr<BaseAST>($1);
    s->isConst = false;
    $$ = s;
  }
  ;

ConstDecl
  : CONST INT ConstDefines ';'{
    auto cd = new ConstDeclAST();
    cd->constDefines = unique_ptr<BaseAST>($3);
    $$ = cd;
  }
  ;



ConstDefines
  : ConstDef{
    auto cd = new ConstDefinesAST();
    cd->constdefList.push_back($1);
    $$ = cd;
  }
  | ConstDefines ',' ConstDef{
    auto ptr = dynamic_cast<ConstDefinesAST*>($1);
    ptr->constdefList.push_back($3);
    $$ = ptr;
  }
  ;

ConstDef 
  : IDENT '=' ConstInitial{
    string name = *($1);
    /*int value = dynamic_cast<ConstInitialAST*>($3)->valueSpread();
    struct entry e;
    e.isConst = true;
    e.number = value;
    e.level = symbolTableStack.size();

    unordered_map<string,entry> st = symbolTableStack.top();
    st.emplace(name, e);
    symbolTableStack.pop();
    symbolTableStack.push(st);*/

    auto cd = new ConstDefAST();
    cd->id = name;
    //cd->value = value;
    cd->constInitial = unique_ptr<BaseAST>($3);
    $$ = cd;
  }
  ;

ConstInitial
  : ConstExp{
    auto ci = new ConstInitialAST();
    ci->constExp = unique_ptr<BaseAST>($1);
    $$ = ci;
  }
  ;

ConstExp 
  : Exp{
    auto ce = new ConstExpAST();
    ce->exp = unique_ptr<BaseAST>($1);
    $$ = ce;
  }
  ;

VarDecl
  : INT VarDefines ';'{
    auto vd = new VarDeclAST();
    vd->varDefines = unique_ptr<BaseAST>($2);
    $$ = vd;
  }
  ;

VarDefines
  : VarDef{
    auto vd = new VarDefinesAST();
    vd->vardefList.push_back($1);
    $$ = vd;
  }
  | VarDefines ',' VarDef{
    auto ptr = dynamic_cast<VarDefinesAST*>($1);
    ptr->vardefList.push_back($3);
    $$ = ptr;
  }
  ;

VarDef
  : IDENT{
    auto s = new VarDefAST();
    s->isInitial = false;
    s->id = *($1);
    $$ = s;
  }
  | IDENT '=' Initial{
    auto s = new VarDefAST();
    s->isInitial = true;
    s->id = *($1);
    s->initial = unique_ptr<BaseAST>($3);
    $$ = s;
  }
  ;

Initial
  : Exp{
    auto s = new InitialAST();
    s->exp = unique_ptr<BaseAST>($1);
    $$ = s;
  }

LVal
  : IDENT{
    $$ = $1;
  }
  ;

Stmt
  : RETURN Exp ';' {
    auto s = new StmtAST();
    s->isReturn = true;
    s->condition = 0;
    s->exp = unique_ptr<BaseAST>($2);
    $$ = s;
  }
  | LVal '=' Exp ';'{
    auto s = new StmtAST();
    s->isReturn = false;
    s->condition = 0;
    s->id = *($1);
    s->exp = unique_ptr<BaseAST>($3);
    $$ = s;
  }
  | RETURN ';'{
    // condition is 1
    auto s = new StmtAST();
    s->condition = 1;
    $$ = s;
  }
  | Block{
    // condition is 2
    auto s = new StmtAST();
    s->condition = 2;
    s->block = unique_ptr<BaseAST>($1);
    $$ = s;
  }
  | Exp ';'{
    // condition is 3
    auto s = new StmtAST();
    s->condition = 3;
    s->exp = unique_ptr<BaseAST>($1);
    s->isReturn = false;
    $$ = s;
  }
  | ';'{
    // condition is 4
    auto s = new StmtAST();
    s->condition = 4;
    $$ = s;
  }
  | WHILE '(' Exp ')' IfStmt {
    // condition is 5
    auto s = new StmtAST();
    s->condition = 5;
    s->exp = unique_ptr<BaseAST>($3);
    s->ifstmt = unique_ptr<BaseAST>($5);
    $$ = s;
  }
  | CONTINUE ';'{
    // condition is 6
    auto s = new StmtAST();
    s->condition = 6;
    $$ = s;
  }
  | BREAK ';'{
    // condition is 7
    auto s = new StmtAST();
    s->condition = 7;
    $$ = s;
  }
  ;

Exp
  : LOrExp{
    auto s = new ExpAST();
    s->lorexp = unique_ptr<BaseAST>($1);
    $$ = s;
  }
  ;

LOrExp
  : LAndExp{
    auto s = new LOrExpAST();
    s->landexpList.push_back($1);
    $$ = s;
  }
  | LOrExp OR LAndExp{
    auto ptr = dynamic_cast<LOrExpAST*>($1);
    ptr->landexpList.push_back($3);
    $$ = ptr;
  }
  ;

LAndExp
  : EqExp{
    auto s = new LAndExpAST();
    s->eqexpList.push_back($1);
    $$ = s;
  }
  | LAndExp AND EqExp{
    auto ptr = dynamic_cast<LAndExpAST*>($1);
    ptr->eqexpList.push_back($3);
    $$ = ptr;
  }
  ;

EqExp
  : RelExp{
    auto s = new EqExpAST();
    s->relexpList.push_back($1);
    $$ = s;
  }
  | EqExp EQUAL RelExp{
    auto ptr = dynamic_cast<EqExpAST*>($1);
    ptr->relexpList.push_back($3);
    ptr->opList.push_back(true);
    $$ = ptr;
  }
  | EqExp NEQUAL RelExp{
    auto ptr = dynamic_cast<EqExpAST*>($1);
    ptr->relexpList.push_back($3);
    ptr->opList.push_back(false);
    $$ = ptr;
  }
  ;

RelExp
  : AddExp{
    auto s = new RelExpAST();
    s->addexpList.push_back($1);
    $$ = s;
  }
  | RelExp '<' AddExp{
    auto ptr = dynamic_cast<RelExpAST*>($1);
    ptr->addexpList.push_back($3);
    ptr->opList.push_back('<');
    $$ = ptr;
  }
  | RelExp '>' AddExp{
    auto ptr = dynamic_cast<RelExpAST*>($1);
    ptr->addexpList.push_back($3);
    ptr->opList.push_back('>');
    $$ = ptr;
  }
  | RelExp LEQUAL AddExp{
    auto ptr = dynamic_cast<RelExpAST*>($1);
    ptr->addexpList.push_back($3);
    ptr->opList.push_back(',');
    $$ = ptr;
  }
  | RelExp GEQUAL AddExp{
    auto ptr = dynamic_cast<RelExpAST*>($1);
    ptr->addexpList.push_back($3);
    ptr->opList.push_back('.');
    $$ = ptr;
  }
  ;

AddExp
  : MulExp{
    auto s = new AddExpAST();
    s->mulexpList.push_back($1);
    $$ = s;
  }
  | AddExp '+' MulExp{
    auto ptr = dynamic_cast<AddExpAST*>($1);
    ptr->mulexpList.push_back($3);
    ptr->opList.push_back('+');
    $$ = ptr;
  }
  | AddExp '-' MulExp{
    auto ptr = dynamic_cast<AddExpAST*>($1);
    ptr->mulexpList.push_back($3);
    ptr->opList.push_back('-');
    $$ = ptr;
  }
  ;

MulExp
  : UnaryExp{
    auto s = new MulExpAST();
    s->unaryexpList.push_back($1);
    $$ = s;
  }
  | MulExp '*' UnaryExp{
    auto ptr = dynamic_cast<MulExpAST*>($1);
    ptr->unaryexpList.push_back($3);
    ptr->opList.push_back('*');
    $$ = ptr;
  }
  | MulExp '/' UnaryExp{
    auto ptr = dynamic_cast<MulExpAST*>($1);
    ptr->unaryexpList.push_back($3);
    ptr->opList.push_back('/');
    $$ = ptr;
  }
  | MulExp '%' UnaryExp{
    auto ptr = dynamic_cast<MulExpAST*>($1);
    ptr->unaryexpList.push_back($3);
    ptr->opList.push_back('%');
    $$ = ptr;
  }
  ;

UnaryExp
  : PrimaryExp {
    auto s = new UnaryExpAST();
    s->primaryexp = unique_ptr<BaseAST>($1);
    $$ = s;
  }
  | UnaryOp UnaryExp{
    dynamic_cast<UnaryExpAST*>($2)->unaryopList.push_back((dynamic_cast<UnaryOpAST*>($1))->unaryop);
    $$ = $2;
  }
  | IDENT '(' ')'{
    auto s = new UnaryExpAST();
    s->callName = *($1);
    $$ = s;
  }
  ;

PrimaryExp
  : '(' Exp ')' {
    auto s = new PrimaryExpAST();
    s->isNum = false;
    s->isVar = false;
    s->exp = unique_ptr<BaseAST>($2);
    $$ = s;
  }
  | Number{
    //cout << "primary - number" << endl;
    auto s = new PrimaryExpAST();
    s->isNum = true;
    s->isVar = false;
    s->number = (dynamic_cast<NumberAST*>($1))->num;
    $$ = s;
  }
  | LVal{
    //cout << "primary - LVal" << endl;
    auto s = new PrimaryExpAST();
    string name = *($1);
    /*entry e;

    //e = searchSymbolTable(name);

    bool isFind = false;
    unordered_map<string,entry>::iterator it;
    unordered_map<string,entry> st;
    stack<unordered_map<string,entry> > garbageStack;
    while(!symbolTableStack.empty()){
      st = symbolTableStack.top();
      it = st.find(name);
      if(it != st.end()){
        e = it->second;
        isFind = true;
        break;
      }
      garbageStack.push(st);
      symbolTableStack.pop();
    }
    while(!garbageStack.empty()){
      unordered_map<string, entry> temp = garbageStack.top();
      garbageStack.pop();
      symbolTableStack.push(temp);
    }

    if(isFind && e.isConst){
      s->isNum = true;
      s->isVar = false;
      s->number = e.number;
    }else{
      s->isNum = false;
      s->isVar = true;
      s->id = name; // + "l" + to_string(e.level); 
    }*/
    s->isNum = false;
    s->isVar = true;
    s->id = name; // + "l" + to_string(e.level); 
    $$ = s;
  }
  ;

UnaryOp
  : '+' {
    $$ = new UnaryOpAST('+');
  }
  | '-' {
    $$ = new UnaryOpAST('-');
  }
  | '!'{
    $$ = new UnaryOpAST('!');
  }
  ;


Number
  : INT_CONST {
    //$$ = new string(to_string($1));
    $$ = new NumberAST($1);
  }
  ;

// 为了避免if-else可能带来的悬空二义性问题，采取了课本上面的文法，以此解决二义性问题
// IfStmt是比Stmt高一级的非终结符
IfStmt
  : Matched_stmt{
    auto s = new IfStmtAST();
    s->isMatched = true;
    s->stmt = unique_ptr<BaseAST>($1);
    $$ = s;
  }
  | Open_stmt{
    auto s = new IfStmtAST();
    s->isMatched = false;
    s->stmt = unique_ptr<BaseAST>($1);
    $$ = s;
  }
  ;

Matched_stmt
  : Stmt{
    auto s = new MatchedStmtAST();
    s->isIf = false;
    s->stmt = unique_ptr<BaseAST>($1);
    $$ = s;
  }
  | IF '(' Exp ')' Matched_stmt ELSE Matched_stmt{
    auto s = new MatchedStmtAST();
    s->isIf = true;
    s->exp = unique_ptr<BaseAST>($3);
    s->thenstmt = unique_ptr<BaseAST>($5);
    s->elsestmt = unique_ptr<BaseAST>($7);
    $$ = s;
  }
  ;

Open_stmt
  : IF '(' Exp ')' Matched_stmt ELSE Open_stmt{
    auto s = new OpenStmtAST();
    s->isElse = true;
    s->exp = unique_ptr<BaseAST>($3);
    s->thenstmt = unique_ptr<BaseAST>($5);
    s->elsestmt = unique_ptr<BaseAST>($7);
    $$ = s;
  }
  | IF '(' Exp ')' IfStmt{
    auto s = new OpenStmtAST();
    s->isElse = false;
    s->exp = unique_ptr<BaseAST>($3);
    s->thenstmt = unique_ptr<BaseAST>($5);
    $$ = s;
  }
  ;





%%

// 定义错误处理函数, 其中第二个参数是错误信息
// parser 如果发生错误 (例如输入的程序出现了语法错误), 就会调用这个函数
void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
  cerr << "error: " << s << endl;
}
