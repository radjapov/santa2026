#define ELF_INTERFACE_CLOSURE_LIBRARY
#include "elf_interface_closure.cpp"

static bool complete(const std::vector<Value>& ready,const std::vector<Value>& targets,Value cap,const Script& p){
    std::unordered_set<Value> seen(ready.begin(),ready.end());
    for(auto x:p){
        if(!seen.count(x.a)||!seen.count(x.b)||calculate(x.op,x.a,x.b,cap)!=x.x||!seen.insert(x.x).second)return false;
    }
    return std::all_of(targets.begin(),targets.end(),[&](auto v){return seen.count(v);});
}

static Value available(Value state,Value cap){
    const auto collect=[&](auto fn){for(Value a=1;a<=cap;++a)if(state&(Value(1)<<a))
        for(Value b=1;b<=cap;++b)if(state&(Value(1)<<b))fn(a,b);};
    Value offer=0;
    collect([&](Value a,Value b){for(char op:{'+','-','*','/'})if(auto x=calculate(op,a,b,cap))offer|=Value(1)<<x;});
    return offer&~state;
}
static bool logical(Value stable,Value roots,int helpers,Value cap){
    while(true){
        Value add=available(stable,cap)&roots;
        if(!add)break;
        stable|=add;
    }
    if((roots&~stable)==0)return true;
    if(!helpers)return false;
    for(Value pool=available(stable,cap);pool;pool&=pool-1){
        if(logical(stable|(pool&(~pool+1)),roots,helpers-1,cap))return true;
    }
    return false;
}

int main(){
    using namespace interface_search;
    Meter fast,full;
    auto cascade=synth({5,7},{12,17,24},27000,0,16,0,91,Deadline::max(),fast);
    if(cascade.size()!=3||!complete({5,7},{12,17,24},27000,cascade))return 1;
    auto blocked=synth({11,13},{1320,1344},27000,2,16,0,13,Deadline::max(),fast);
    if(!blocked.empty())return 2;
    auto found=synth({11,13},{1320,1344},27000,2,16,2,13,Deadline::max(),full);
    if(found.size()!=4||!complete({11,13},{1320,1344},27000,found)||!full.dormant)return 3;
    auto low=synth({11,13},{1320,1344},27000,1,16,2,13,Deadline::max(),full);
    if(!low.empty())return 4;
    for(int cap=0;cap<3;++cap){
        auto exact=synth({11,13},{1320,1344},27000,cap,5,6,13,Deadline::max(),full);
        if(cap<2&&!exact.empty()||cap==2&&!complete({11,13},{1320,1344},27000,exact))return 6;
    }
    for(Value t=4;t<500;++t){
        for(int j=1;j<=3;++j){
            if(j==2&&2*t>700)continue;
            const auto a=synth({6,8,31},{t,t+j},700,3,11,0,t+31,Deadline::max(),fast);
            const auto b=synth({6,8,31},{t,t+j},700,3,11,3,t+31,Deadline::max(),full);
            if(a.size()!=b.size()||!b.empty()&&!complete({6,8,31},{t,t+j},700,b))return 5;
        }
    }
    const Value tiny=35,seeds=(Value(1)<<4)|(Value(1)<<9)|(Value(1)<<11);
    for(Value t=1;t<=tiny;++t)for(Value u=t+1;u<=tiny;++u){
        if((seeds&(Value(1)<<t))&&(seeds&(Value(1)<<u)))continue;
        for(int cap=0;cap<=2;++cap){
            const auto b=synth({4,9,11},{t,u},tiny,cap,5,6,t^u,Deadline::max(),full);
            if(logical(seeds,(Value(1)<<t)|(Value(1)<<u),cap,tiny)!=(!b.empty()))return 7;
            if(!b.empty()&&!complete({4,9,11},{t,u},tiny,b))return 8;
        }
    }
    Graph graph;
    graph.initial.fill(-1);
    graph.initial[0]=0;graph.initial[1]=1;graph.initial[2]=2;
    graph.nodes={{5,0,-1,-1},{7,0,-1,-1},{11,0,-1,-1},{22,'+',2,2},
                 {17,'-',3,0},{35,'*',0,1},{24,'-',5,2},{121,'*',2,2},{847,'*',7,1}};
    graph.targets={4,6,8};
    const auto cut=make_cut(graph,graph.targets);
    Unions retire(graph,cut);
    State strand({5,7,11},3);
    strand.put({12,'+',5,7},true);
    closure(strand,{17,24,847},1200,3);
    retire.observe(strand);
    if(retire.plans.size()!=1||retire.plans[0].cut.roots.size()!=2||retire.plans[0].recipe.size()!=3)return 9;
    auto changed=replace_block(graph,retire.plans[0].cut,retire.plans[0].recipe,1200,6,12);
    if(changed.size()!=5)return 10;
    std::cout<<"Verified compulsory exits, shared four-write completion, and dormant-helper ablation\n";
}
