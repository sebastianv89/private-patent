import xml.etree.ElementTree as x
from collections import Counter
import math

d = []
s = ""
#with open("ipg161011-small.xml", 'r') as f:
with open("ipg161011.xml", 'r') as f:
    for l in f:
        if l.startswith('<?xml'):
            if len(s)>0:
                d.append(s)
            s = ""
        s += l
    d.append(s)

def get_words(text):
    return ''.join((c if c.isalpha() else ' ') for c in text.lower()).split()

def hist(node, counter):
    for child in node:
        counter = hist(child, counter)
    t = node.text
    if t is None:
        return counter
    counter.update(get_words(t))
    return counter

def get_class(root):
    lvl1 = root.find('us-bibliographic-data-grant')
    if lvl1 is None:
        return None
    lvl2 = lvl1.find('classifications-cpc')
    if lvl2 is None:
        return None
    lvl3 = lvl2.find('main-cpc')
    if lvl3 is None:
        return None
    lvl4 = lvl3.find('classification-cpc')
    if lvl4 is None:
        return None
    lvl5 = lvl4.find('section')
    if lvl5 is None:
        return None
    return lvl5.text

patents_dict = {}
patent_count = 0.0
for xm in d[:len(d)/2]:
    root = x.fromstring(xm)
    # get cpc classification (if it exists)
    cpc_class = get_class(root)
    if cpc_class is None:
        continue
    # get abstract histogram (if abstract exists)
    abstract = root.find('abstract')
    if abstract is None:
        continue
    hist_abstr = hist(abstract, Counter())
    # update the class data
    try:
        cl = patents_dict[cpc_class]
        cl[0] += 1
        cl[1].update(hist_abstr)
    except KeyError:
        patents_dict[cpc_class] = [1, hist_abstr, 0]
    patent_count += 1.0
for k, v in patents_dict.iteritems():
    v[2] = sum(v[1].values())
patents = sorted(patents_dict.items())

# create a list of all words
all_words_counter = Counter()
for _, v in patents:
    all_words_counter += v[1]
total_words = float(sum(all_words_counter.values()))
all_words = sorted(all_words_counter.items())
unique_words = float(len(all_words))

print 'Total words:', total_words
print 'Unique words:', unique_words

with open('patents.names', 'w') as of:
    of.write('Patent data\n\n')
    of.write('Total words: ' + str(int(total_words)) + '\n')
    of.write('Unique words: ' + str(int(unique_words)) + '\n')
    of.write('List of all classes (with their total count):\n\n')
    for cl, v in patents:
        of.write(cl + ' (' + str(v[0]) + ')\n')
    of.write('\nList of all words in the patents (with their total count):\n\n')
    for w in all_words:
        of.write(w[0].encode('utf-8') + ' (' + str(w[1]) + ')\n');

def prior(class_count):
    return math.log(class_count / patent_count)

def cond(word, hist, hist_sum):
    p = (hist[word] + 1.0) / (hist_sum + unique_words)
    return math.log(p), math.log(1-p)
    
with open('patents.nb.model', 'w') as of:
    # prior probabilities
    of.write('{\n"prior": ')
    first = True
    for _, v in patents:
        if first:
            of.write('[')
            first = False
        else:
            of.write(', ')
        of.write(str(prior(v[0])))
    of.write('],\n')
    
    # conditional probabilities
    of.write('"conditionals": [\n  ')
    firstPatent = True
    for _, v in patents:
        if firstPatent:
            firstPatent = False
            of.write('[\n    ')
        else:
            of.write(',\n  [\n    ')
        firstWord = True
        for w, _ in all_words:
            if firstWord:
                firstWord = False
            else:
                of.write(',\n    ')
            p, q = cond(w, v[1], v[2])
            of.write('[' + str(p) + ', ' + str(q) + ']')
        of.write('\n  ]')
    of.write('\n]\n}\n')
