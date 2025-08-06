#pragma once
#include <atomic>
#include <memory>

#define SET_PTR(cnp,p) do{\
    cnp&=-1L<<48;\
    cnp|=(uint64_t)(p);\
}while(0)
#define GET_PTR(cnp) ((node*)(cnp&~(-1L<<48)))
#define SET_COUNT(cnp,c) do{\
    cnp&=~(-1L<<48);\
    cnp|=(uint64_t)(c)<<48;\
}while(0)
#define GET_COUNT(cnp) (((uint64_t)cnp)>>48)


// 使用指针存储，new分配
// 无数据返回空unique_ptr
// 析构时free所有指针
template<typename T>
class LFQueue{
private:
    typedef uint64_t counted_node_ptr;// 低48位存储指针，高16位存储引用计数
    std::atomic<counted_node_ptr> head;
    std::atomic<counted_node_ptr> tail;    

    struct node_counter{
        unsigned internal_count:30;
        unsigned external_counters:2;   
    };
    struct node{
        std::atomic<T*> data;
        std::atomic<node_counter> count;    
        std::atomic<counted_node_ptr> next;
        node(){
            node_counter new_count;
            new_count.internal_count=0;
            new_count.external_counters=2;    
            count.store(new_count);

            counted_node_ptr node_ptr=0;
            next.store(node_ptr);
        }

        // 内部计数器-1
        void release_ref(){
            node_counter old_counter=
                count.load(std::memory_order_relaxed);
            node_counter new_counter;

            do{
                new_counter=old_counter;
                --new_counter.internal_count;    
            }while(!count.compare_exchange_strong(    
                  old_counter,new_counter,
                  std::memory_order_acquire,std::memory_order_relaxed));

            if(!new_counter.internal_count &&
               !new_counter.external_counters){
                delete this;    
                destruct_count.fetch_add(1);
            }
        }
    };

    // cnp中外部计数器+1
    static void increase_external_count(
        std::atomic<counted_node_ptr>& counter,counted_node_ptr& old_counter){
        counted_node_ptr new_counter;
        
        do{
            new_counter=old_counter;
            auto count=GET_COUNT(new_counter)+1;
            SET_COUNT(new_counter,count);
        }while(!counter.compare_exchange_strong(
              old_counter,new_counter,
              std::memory_order_acquire,std::memory_order_relaxed));
              
        SET_COUNT(old_counter,GET_COUNT(new_counter));
    }

    // cnp中外部计数器-2后加到内部计数器中，同时内部ec-1
    static void free_external_counter(counted_node_ptr &old_node_ptr){
        node* const ptr=GET_PTR(old_node_ptr);
        int const count_increase=GET_COUNT(old_node_ptr)-2;
        node_counter old_counter=
            ptr->count.load(std::memory_order_relaxed);
        node_counter new_counter;

        do{
            new_counter=old_counter;
            --new_counter.external_counters;
            new_counter.internal_count+=count_increase;    
        }while(!ptr->count.compare_exchange_strong(    
              old_counter,new_counter,
              std::memory_order_acquire,std::memory_order_relaxed));

        if(!new_counter.internal_count &&
           !new_counter.external_counters){
            delete ptr;
            destruct_count.fetch_add(1);
        }
    }

    void set_new_tail(counted_node_ptr &old_tail,   
                      counted_node_ptr const &new_tail){
        node* const current_tail_ptr=GET_PTR(old_tail);
        
        while(!tail.compare_exchange_weak(old_tail,new_tail) &&   
              GET_PTR(old_tail)==current_tail_ptr);
   
        if(GET_PTR(old_tail)==current_tail_ptr)
            // 成功修改tail的线程释放
            free_external_counter(old_tail);    
        else
            current_tail_ptr->release_ref();    
    }
public:
    static std::atomic<int> destruct_count;
    std::atomic<int> size=0;
    std::atomic<int> push_count=0;
    std::atomic<int> pop_count=0;


    LFQueue(){
        counted_node_ptr new_next;
        SET_PTR(new_next,new node());
        SET_COUNT(new_next,1);
        tail.store(new_next);
        head.store(new_next);
    }
    ~LFQueue(){
        while (pop());
        auto head_counted_node = head.load();
        delete GET_PTR(head_counted_node);
    }

    void push(std::unique_ptr<T> new_data){
        counted_node_ptr new_next;
        SET_PTR(new_next,new node());
        SET_COUNT(new_next,1);
        counted_node_ptr old_tail=tail.load();
        for(;;){
            increase_external_count(tail,old_tail);    
            T* old_data=nullptr;

            auto node_ptr=GET_PTR(old_tail);
            
            if(node_ptr->data.compare_exchange_strong(   
               old_data,new_data.get())){
                // 竞争更新next 
                counted_node_ptr old_next={0};
                if(!node_ptr->next.compare_exchange_strong(old_next,new_next)){
                    // 竞争失败，因为已成功push数据，后续不再需要
                    delete GET_PTR(new_next);
                    new_next=old_next;
                }

                // 竞争更新tail
                set_new_tail(old_tail,new_next);

                new_data.release();

                push_count.fetch_add(1);
                size.fetch_add(1);
                break;
            }else{
                // 竞争更新next 
                counted_node_ptr old_next={0};
                if(node_ptr->next.compare_exchange_strong(old_next,new_next)){
                    old_next=new_next;
                    // 竞争成功，因为未成功push数据，后续还需要
                    SET_PTR(new_next,new node());
                }

                // 竞争更新tail
                set_new_tail(old_tail,old_next);
            }
        }
    }

    std::unique_ptr<T> pop(){
        // 防止无数据时循环pop导致计数器迅速溢出
        if(GET_PTR(head.load())==GET_PTR(tail.load())){
            return std::unique_ptr<T>();
        }

        counted_node_ptr old_head=head.load(std::memory_order_relaxed);    

        for(;;){
            increase_external_count(head,old_head);

            node* const ptr=GET_PTR(old_head);

            if(ptr==GET_PTR(tail.load())){
                ptr->release_ref();    
                return std::unique_ptr<T>();
            }

            counted_node_ptr next=ptr->next.load();
            if(head.compare_exchange_strong(old_head,next)){
                T* const res=ptr->data.exchange(nullptr);
                free_external_counter(old_head);   

                pop_count.fetch_add(1);
                size.fetch_sub(1);
                return std::unique_ptr<T>(res);
            }
            
            ptr->release_ref();    
        }
    }
};

template<typename T>
std::atomic<int> LFQueue<T>::destruct_count=0;