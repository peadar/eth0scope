CXXFLAGS += -g

pa: pa.o
	$(CXX) -o $@ $^ -lpulse


clean:
	rm -f *.o core pa
